#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/vm_manager/timer.hpp>

#include <stdexcept>

namespace koinos::chain {

struct frame_guard
{
  frame_guard( execution_context& ctx, stack_frame&& f ):
      _ctx( ctx )
  {
    _ctx.push_frame( std::move( f ) );
  }

  ~frame_guard()
  {
    _ctx.pop_frame();
  }

private:
  execution_context& _ctx;
};

struct block_guard
{
  block_guard( execution_context& context, const protocol::block& block ):
      ctx( context )
  {
    ctx.set_block( block );
  }

  ~block_guard()
  {
    ctx.clear_block();
  }

  execution_context& ctx;
};

bool validate_utf( const bytes_s str )
{
  auto it = str.begin();
  while( it != str.end() )
  {
    const boost::locale::utf::code_point cp = boost::locale::utf::utf_traits< std::byte >::decode( it, str.end() );
    if( cp == boost::locale::utf::illegal )
      return false;
    else if( cp == boost::locale::utf::incomplete )
      return false;
  }
  return true;
}

execution_context::execution_context( std::shared_ptr< vm_manager::vm_backend > vm_backend, chain::intent intent ):
    _vm_backend( vm_backend ),
    _intent( intent ),
{}

std::shared_ptr< vm_manager::vm_backend > execution_context::get_backend() const
{
  return _vm_backend;
}

void execution_context::set_state_node( abstract_state_node_ptr node )
{
  _state_node = node;
}

void execution_context::clear_state_node()
{
  _state_node.reset();
}

koinos_error execution_context::apply_block( const protocol::block& block )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _receipt = protocol::block_receipt;
  _block = &block;


  auto resource_limits_object = _state_node->get_object( state::space::metadata(), state::key::resource_limit_data );
  if( resource_limits_object != nullptr )
    throw std::runtime_error( "resource limit data does not exist" );

  _resource_meter.set_resource_limit_data( util::converter::to< resource_limit_data >( resource_limit_object ) );

  KOINOS_CHECK_ERROR(
    crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( block.header() ) )
      == block.id(),
    error_code::malformed_block,
    "block id is invalid" );

  // Check transaction merkle root
  std::vector< std::string > hashes;
  hashes.reserve( block.transactions_size() * 2 );

  std::size_t transactions_bytes_size = 0;
  for( const auto& trx: block.transactions() )
  {
    transactions_bytes_size += trx.ByteSizeLong();
    hashes.emplace_back( crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( trx.header() ) ) );

    std::stringstream ss;

    hashes.emplace_back( crypto::hash( crypto::multicodec::sha2_256, ss.str() ) );
  }

  KOINOS_CHECK_ERROR(
    crypto::merkle_tree( crypto::multihash::sha2_256, hashes ),
      == util::converter::to< crypto::multihash >( block.header().transaction_merkle_root() ),
    error_code::malformed_block,
    "transaction merkle root does not match"
  );

  // Process block signature
  auto genesis_address = _state_node.get_object( state::space::metadata(), state::key::genesis_key );
  if( !genesis_address )
    throw std::runtime_error( "genesis address not found" );

  auto block_hash = crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( block.header() ) );
  KOINOS_CHECK_ERROR(
    crypto::recover_public_key( ecdsa_secp256k1, block.signature_data(), block.id(), true ).to_address_bytes()
      == genesis_address,
    error_code::invalid_signature,
    "failed to process block signature" );

  for( const auto& trx : block.transactions() )
  {
    auto error = apply_transaction( trx );

    if( is_failure( error ) )
      return error;
  }

  const auto& rld = _resource_meter.get_resource_limit_data();

  uint64_t system_rc_cost = _resource_meter.system_disk_storage_used() * rlc.disk_storage_cost()
                            + _resource_meter.system_network_bandwidth_used() * rld.network_bandwidth_cost()
                            + _resource_meter.system_compute_bandwidth_used() * rld.compute_bandwidth_cost();
  // Consume RC is empty

  // Consume block resources is empty

  generate_receipt( *this,
                    std::get< protocol::block_receipt >( _receipt ),
                    block,
                    disk_storage_used,
                    network_bandwidth_used,
                    compute_bandwidth_used );

  return {};
}

koinos_error execution_context::apply_transaction( const protocol::transaction& trx )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _trx = trx;

  bytes_s payer = bytes_s( static_cast< std::byte* >( trx.header().payer().data() ),
                           static_cast< std::byte* >( trx.header().payer().data() + trx.header().payer().size() ) );
  bytes_s payee = bytes_s( static_cast< std::byte* >( trx.header().payee().data() ),
                           static_cast< std::byte* >( trx.header().payee().data() + trx.header().payee().size() ) );

  uint64_t payer_rc, used_rc, disk_storage_used, compute_bandwidth_used, network_bandwidth_used = 0;
  std::vector< protocol::event_data > events;
  std::vector< std::string > logs;

  bool use_payee_nonce = payee.size() && payee != payer;
  auto nonce_account = use_payee_nonce ? payee : payer;

  auto start_disk_used = _resource_meter.disk_storage_used();
  auto start_network_used = _resource_meter.network_bandwidth_used();
  auto start_compute_used = _resource_meter.compute_bandwidth_used();

  auto payer_session = make_session( trx.header().rc_limit() );

  uint64_t payer_rc = 1'000'000'000;

  auto error = [&]() -> koinos_error
  {
    KOINOS_CHECK_ERROR(
      payer_rc >= trx.header().rc_limit(),
      error_code::failure,
      "payer does not have rc to cover transaction rc limit" );

    auto chain_id = _state_node->get_object( state::space::metadata(), state::key::chain_id );
    if( chain_id == nullptr )
      throw std::runtime_error( "could not find chain id" );

    KOINOS_CHECK_ERROR(
      trx.header().chain_id() == *chain_id,
      error_code::failure,
      "chain id mismatch" );

    KOINOS_CHECK_ERROR(
      crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( trx.header() ) )
        == trx.id(),
      error_code::failure,
      "transaction id is invalid" );

    std::vector< std::string > hashes;
    hashes.reserve( trx.operations_size() );

    for( const auto& op: trx.operation() )
      hashes.emplace_back( crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( op ) ) );

    KOINOS_CHECK_ERROR(
      crypto::merkle_tree( crypto::multihash::sha2_256, hashes ),
        == util::converter::to< crypto::multihash >( trx.header().operation_merkle_root() ),
      error_code::failure_exception,
      "operation merkle root does not match" );

    auto error = check_authority( payer );
    KOINOS_CHECK_ERROR( !error, error_code::authorization_failure, "payer has not authorized transaction" );

    if( use_payee_nonce )
    {
      error = check_authority( payee );
      KOINOS_CHECK_ERROR( !error, error_code::authorization_failure, "payee has not authorized transaction" );
    }

    error = check_account_nonce( nonce_account );
    KOINOS_CHECK_ERROR( !error, error_code::invalid_nonce, "invalid transaction nonce" );

    error = set_account_nonce( nonce_account, trx.header().nonce() );
    if( error )
      return error;

    _resource_meter.use_network_bandwidth( trx.ByteSizeLong() );
  }();

  // All errors are failures here
  if( error )
    return error;

  auto block_node = _state_node;
  _state_node = block_node->create_anonymous_node();

  error = [&]() -> koinos_error {
    for( const auto& o: trx.operations() )
    {
      _op = o;

      if( o.has_upload_contract() )
      {

      }
      else if( o.has_call_contract() )
      {
        // TODO: parse entry point and arguments and call call_contract
      }
      else
        KOINOS_CHECK_ERROR( false, error_code::unknown_operation, "unknown operation" );
    }

    _state_node->commit();
    return {};
  }();

  protocol::transaction_receipt receipt;
  _state_node = block_node;

  if( is_failure( error ) )
    return error;
  if( is_reversion( error ) )
  {
    receipt.set_reverted( true );
    log( "transaction reverted: " + error.message() );
  }

  auto used_rc = payer_session->used_rc();
  auto logs    = payer_session->logs();
  auto events  = receipt.reverted() ? std::vector< protocol::event >() : payer_session->events();

  auto disk_storage_uses      = _resource_meter.disk_storage_used() - start_disk_used;
  auto network_bandwidth_used = _resource_meter.network_bandwidth_used() - start_network_used;
  auto compute_bandwidth_used = _resource_meter.compute_bandwidth_used() - start_compute_used;

  payer_session.reset();

  auto consume_error = consume_account_rc( payer, used_rc );

  generate_receipt( receipt,
                    trx,
                    payer_rc,
                    used_rc,
                    disk_storage_used,
                    network_bandwidth_used,
                    compute_bandwidth_used,
                    events,
                    logs );

  switch( _intent )
  {
    case intent::block_application:
      if( !std::holds_alternative< protocol::block_receipt >( _receipt ) )
        throw std::runtime_exception( "expected block receipt with block application intent" );
      *std::get< protocol::block_receipt >( _receipt ).add_transaction_receipts() = receipt;
      break;
    case intent::transaction_application:
      _receipt = receipt;
      break;
    default:
      assert( false );
      break;
  }

  return error ? error : consume_error;
}

koinos_error apply_operation( const protocol::operation& )
{
  return {};
}

koinos_error check_account_nonce( bytes_s account, bytes_s nonce )
{
  return {};
}

resource_meter& execution_context::resource_meter()
{
  return _resource_meter;
}

chronicler& execution_context::chronicler()
{
  return _chronicler;
}

chain::receipt& execution_context::receipt()
{
  return _receipt;
}

void execution_context::push_frame( stack_frame&& frame )
{
  KOINOS_ASSERT( _stack.size() < execution_context::stack_limit,
                 chain::reversion_exception,
                 "apply context stack overflow" );
  _stack.emplace_back( std::move( frame ) );
}

stack_frame execution_context::pop_frame()
{
  KOINOS_ASSERT( _stack.size(), chain::internal_error_exception, "stack is empty" );
  auto frame = _stack[ _stack.size() - 1 ];
  _stack.pop_back();
  return frame;
}

std::shared_ptr< session > execution_context::make_session( uint64_t rc )
{
  auto session = std::make_shared< chain::session >( rc );
  resource_meter().set_session( session );
  chronicler().set_session( session );
  return session;
}

std::expected< std::vector< bytes_v >&, koinos_error > execution_context::arguments()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "context stack is empty" );

  return _stack.rbegin()->arguments;
}

koinos_error execution_context::write_output( bytes_s bytes )
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "context stack is empty" );

  auto& output = _stack.rbegin()->output;

  output.insert( output.end(), bytes.begin(), bytes.end() );

  return {};
}

std::expected< object_space, koinos_error > execution_context::create_object_space( uint32_t id )
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "context stack is empty" );

  object_space space;
  space.set_id( id );
  space.set_zone( _stack.rbegin()->contract_id );
  space.set_system( false );

  return space;
}

std::expected< bytes_s, koinos_error > execution_context::get_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  auto result = _state_node->get_object( create_object_space( id ), key );

  if( result )
    return bytes_s( static_cast< std::byte* >( result->data() ),
                    static_cast< std::byte* >( result->data() + result->size() ) );

  return bytes_s();
}

std::expected< std::pair< bytes_s, bytes_v >, koinos_error > execution_context::get_next_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  const auto [ result, next_key ] = _state_node->get_next_object( create_object_space( id ), key );

  if( result )
    std::make_pair( bytes_s( static_cast< std::byte* >( result->data() ),
                             static_cast< std::byte* >( result->data() + result->size() ) ),
                    std::move( next_key ) );

  return make_pair( bytes_s(), bytes_v() );
}

std::expected< bytes_s, koinos_error > execution_context::get_prev_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  const auto [ result, prev_key ] = _state_node->get_prev_object( create_object_space( id ), key );

  if( result )
    std::make_pair( bytes_s( static_cast< std::byte* >( result->data() ),
                             static_cast< std::byte* >( result->data() + result->size() ) ),
                    std::move( prev_key ) );

  return make_pair( bytes_s(), bytes_v() );
}

koinos_error execution_context::put_object( uint32_t id, bytes_s key, bytes_s value )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _resource_meter.use_disk_storage( _state_node->put_object( create_object_space( id ), key, value ) );

  return {};
}

koinos_error execution_context::remove_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _resource_meter.use_disk_storage( _state_node->remove_object( create_object_space( id ), key ) );
}

koinos_error execution_context::log( bytes_s message )
{
  _chronicler.push_log( message );

  return {};
}

koinos_error execution_context::event( bytes_s name, bytes_s data, std::vector< account_t >& impacted )
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "context stack is empty" );

  KOINOS_CHECK_ERROR( name.size(), error_code::reversion, "event name cannot be empty" );
  KOINOS_CHECK_ERROR( name.size() <= 128, error_code::reversion, "event name cannot be larger than 128 bytes" );
  KOINOS_CHECK_ERROR( validate_utf( name ), error_code::reversion, "event name contains invalid utf-8" );

  protocol::event_data ev;
  ev.set_source( _stack.rbegin()->contract_id );
  ev.set_name( name );
  ev.set_data( std::string( reinterpret_cast< char* >( data.data() ), data.size() ) );

  for( auto& imp: impacted )
    *ev.add_impacted = std::string( reinterpret_cast< char* >( imp.data() ), imp.size() );

  _chronicler.push_event( _trx ? _trx->id() : {}, std::move( ev ) );
}

std::expected< bool, koinos_error > execution_context::check_authority( bytes_s account )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  KOINOS_CHECK_ERROR( _intent == intent::read_only, error_code::reversion, "check authority is forbidden in read only" );

  const auto contract_meta_bytes = _state_node->get_object( state::space::contract_metadata(), address );
  // Contract case
  if( contract_meta_bytes.size() )
  {
    std::vector< bytes_s > authorize_args;

    // Calling from within a contract
    if( _stack.size() > 0 )
    {
      const auto& frame = *_stack.rbegin();

      bytes_v entry_point_bytes( static_cast< std::byte* >( &frame.entry_point ),
                                static_cast< std::byte* >( &frame.entry_point ) + sizeof( frame.entry_point ) );

      authorize_args.reserve( frame.arguments.size() + 1 );
      authorize_args.emplace( bytes_s( entry_point_bytes.begin(), entry_point_bytes.end() ) );

      for( const auto& arg : frame.arguments )
        authorize_args.emplace( bytes_s( arg.begin(), arg.end() ) );
    }

    return call_program_priviledged( address, authorize_entrypoint, authorize_args );
  }
  // Raw address case
  else
  {
    if( _trx == nullptr )
      throw std::runtime_error( "transaction required for check authority" );

    for( const auto& arg: _trx->signature() )
    {
      auto signer_address system_call::recover_public_key( context, ecdsa_secps56k1, sig, trx->id(), true ).to_address_bytes();
      if( std::equal( signer_address.begin(), signer_address.end(), account.begin() ) )
        return true;
    }
  }

  return false;
}

std::expected< account_t, koinos_error > execution_context::get_caller()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "context stack is empty" );

  if( _stack.size() == 1 )
    return account_t();

  return _stack[ _stack.size() - 2 ].contract_id;
}

std::expected< bytes_v, koinos_error > execution_context::call_program( bytes_s address, uint32_t entry_point, std::span< bytes_s > args )
{
  KOINOS_CHECK_( entry_point != authorize_entrypoint, error_code::insufficient_priviledges, "user cannot call authorize directly" );

  return call_program_priviledged( address, entry_point, args );
}

std::expected< bytes_v, koinos_error > execution_context::call_program_priviledged( bytes_s address, uint32_t entry_point, std::span< bytes_s > args )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  const auto contract = _state_node->get_object( state::space::contract_bytecode(), address );
  KOINOS_CHECK_ERROR( contract.exists(), error_code::invalid_contract, "contract not found" );

  const auto contract_meta_bytes = _state_node->get_object( state::space::contract_metadata(), address );
  if( !contract_meta_bytes.exists() )
    throw std::runtime_error( "contract metadata does not exist" );
  auto contract_meta = util::converter::to< contract_metadata_object >( contract_meta_bytes.value() );
  if( !contract_meta.hash().size() )
    throw std::runtime_error( "contract hash does not exist" );

  std::vector< bytes_v > args_v;
  args_v.reserve( args.size() );

  for( auto& arg: args )
    args_v.emplace_back( bytes_v( arg.begin(), arg.end() ) );

  push_frame({
    .contract_id = address,
    .arguments = std::move( args_v ),
    .entry_point = entry_point
  });

  auto error = _vm_backend->run( host_api( *this ), contact.value(), contract_meta.hash() );

  auto frame = pop_frame();

  if( error )
    return error;

  return frame.output;
}

} // namespace koinos::chain
