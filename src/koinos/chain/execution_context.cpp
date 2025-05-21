#include <boost/locale/utf.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/crypto/public_key.hpp>
#include <koinos/log/log.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <stdexcept>

namespace koinos::chain {

using koinos::error::error_code;

template< typename T >
bool validate_utf( std::basic_string_view< T > str )
{
  auto it = str.begin();
  while( it != str.end() )
  {
    const boost::locale::utf::code_point cp = boost::locale::utf::utf_traits< T >::decode( it, str.end() );
    if( cp == boost::locale::utf::illegal )
      return false;
    else if( cp == boost::locale::utf::incomplete )
      return false;
  }
  return true;
}

execution_context::execution_context( std::shared_ptr< vm_manager::vm_backend > vm_backend, chain::intent intent ):
    _vm_backend( vm_backend ),
    _intent( intent )
{}

void execution_context::set_state_node( abstract_state_node_ptr node )
{
  _state_node = node;
}

void execution_context::clear_state_node()
{
  _state_node.reset();
}

std::expected< protocol::block_receipt, error > execution_context::apply_block( const protocol::block& block )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  protocol::block_receipt receipt;
  _resource_meter.set_resource_limit_data( get_resource_limits() );

  auto start_resources = _resource_meter.remaining_resources();

  auto block_id = util::converter::to< crypto::multihash >( block.id() );

  if( auto hash = crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( block.header() ) );
      hash )
  {
    if( *hash != block_id )
      return std::unexpected( error_code::malformed_block );
  }
  else
    return std::unexpected( hash.error() );

  // Check transaction merkle root
  std::vector< crypto::multihash > hashes;
  hashes.reserve( block.transactions_size() * 2 );

  std::size_t transactions_bytes_size = 0;
  for( const auto& trx: block.transactions() )
  {
    transactions_bytes_size += trx.ByteSizeLong();
    if( auto hash = crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( trx.header() ) );
        hash )
      hashes.emplace_back( std::move( *hash ) );
    else
      return std::unexpected( hash.error() );

    std::stringstream ss;
    for( const auto& sig: trx.signatures() )
      ss << sig.signature();

    if( auto hash = crypto::hash( crypto::multicodec::sha2_256, ss.str() ); hash )
      hashes.emplace_back( std::move( *hash ) );
    else
      return std::unexpected( hash.error() );
  }

  if( auto tree = crypto::merkle_tree< crypto::multihash >::create( crypto::multicodec::sha2_256, hashes ); tree )
  {
    if( tree->root()->hash() != util::converter::to< crypto::multihash >( block.header().transaction_merkle_root() ) )
      return std::unexpected( error_code::malformed_block );
  }
  else
    return std::unexpected( tree.error() );

  // Process block signature
  auto genesis_bytes_str = _state_node->get_object( state::space::metadata(), state::key::genesis_key );
  if( !genesis_bytes_str )
    throw std::runtime_error( "genesis address not found" );

  auto genesis_public_bytes = util::converter::as< crypto::public_key_data >( *genesis_bytes_str );
  crypto::public_key genesis_key( genesis_public_bytes );

  if( block.signature().size() != sizeof( crypto::signature ) )
    return std::unexpected( error_code::invalid_signature );

  crypto::signature signature = util::converter::as< crypto::signature >( block.signature() );

  crypto::public_key_data public_bytes = util::converter::as< crypto::public_key_data >( block.header().signer() );
  crypto::public_key signer_key( public_bytes );

  if( !signer_key.verify( signature, block_id ) )
    return std::unexpected( error_code::invalid_signature );

  if( signer_key != genesis_key )
    return std::unexpected( error_code::invalid_signature );

  const auto serialized_block = util::converter::as< std::string >( block );
  _state_node->put_object( state::space::metadata(), state::key::head_block, &serialized_block );

  for( const auto& trx: block.transactions() )
  {
    auto trx_receipt = apply_transaction( trx );

    if( trx_receipt )
      *receipt.add_transaction_receipts() = trx_receipt.value();
    else if( trx_receipt.error().is_failure() )
      return std::unexpected( trx_receipt.error() );
  }

  const auto& rld            = _resource_meter.get_resource_limit_data();
  auto system_resources      = _resource_meter.system_resources();
  auto end_charged_resources = _resource_meter.remaining_resources();

  uint64_t system_rc_cost = system_resources.disk_storage * rld.disk_storage_cost()
                            + system_resources.network_bandwidth * rld.network_bandwidth_cost()
                            + system_resources.compute_bandwidth * rld.compute_bandwidth_cost();
  // Consume RC is currently empty

  // Consume block resources is currently empty

  auto end_resources = _resource_meter.remaining_resources();

  receipt.set_id( block.id() );
  receipt.set_height( block.header().height() );
  receipt.set_disk_storage_used( start_resources.disk_storage - end_resources.disk_storage );
  receipt.set_network_bandwidth_used( start_resources.network_bandwidth - end_resources.network_bandwidth );
  receipt.set_compute_bandwidth_used( start_resources.compute_bandwidth - end_resources.compute_bandwidth );
  receipt.set_disk_storage_charged( start_resources.disk_storage - end_charged_resources.disk_storage );
  receipt.set_network_bandwidth_charged( start_resources.network_bandwidth - end_charged_resources.network_bandwidth );
  receipt.set_compute_bandwidth_charged( start_resources.compute_bandwidth - end_charged_resources.compute_bandwidth );

  for( const auto& [ transaction_id, event ]: _chronicler.events() )
    if( !transaction_id )
      *receipt.add_events() = event;

  for( const auto& message: _chronicler.logs() )
    *receipt.add_logs() = message;

  for( const auto& entry: _state_node->get_delta_entries() )
    *receipt.add_state_delta_entries() = entry;

  return receipt;
}

std::expected< protocol::transaction_receipt, error >
execution_context::apply_transaction( const protocol::transaction& trx )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _trx = &trx;
  _recovered_signatures.clear();

  bytes_s payer =
    bytes_s( reinterpret_cast< const std::byte* >( trx.header().payer().data() ),
             reinterpret_cast< const std::byte* >( trx.header().payer().data() + trx.header().payer().size() ) );
  bytes_s payee =
    bytes_s( reinterpret_cast< const std::byte* >( trx.header().payee().data() ),
             reinterpret_cast< const std::byte* >( trx.header().payee().data() + trx.header().payee().size() ) );

  bool use_payee_nonce = payee.size() && !std::equal( payee.begin(), payee.end(), payer.begin(), payer.end() );
  auto nonce_account   = use_payee_nonce ? payee : payer;

  auto start_resources = _resource_meter.remaining_resources();
  auto payer_session   = make_session( trx.header().rc_limit() );

  auto payer_rc = get_account_rc( payer );

  if( payer_rc < trx.header().rc_limit() )
    return std::unexpected( error_code::failure );

  auto chain_id = get_chain_id();
  bytes_s trx_chain_id( reinterpret_cast< const std::byte* >( trx.header().chain_id().data() ),
                        reinterpret_cast< const std::byte* >( trx.header().chain_id().data() )
                          + trx.header().chain_id().size() );

  if( !std::equal( chain_id.begin(), chain_id.end(), trx_chain_id.begin(), trx_chain_id.end() ) )
    return std::unexpected( error_code::failure );

  if( auto hash = crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( trx.header() ) );
      hash )
  {
    if( *hash != util::converter::to< crypto::multihash >( trx.id() ) )
      return std::unexpected( error_code::failure );
  }
  else
    return std::unexpected( error_code::failure );

  std::vector< crypto::multihash > hashes;
  hashes.reserve( trx.operations_size() );

  for( const auto& op: trx.operations() )
    if( auto hash = crypto::hash( crypto::multicodec::sha2_256, util::converter::as< std::string >( op ) ); hash )
      hashes.emplace_back( std::move( *hash ) );
    else
      return std::unexpected( error_code::failure );

  if( auto tree = crypto::merkle_tree< crypto::multihash >::create( crypto::multicodec::sha2_256, hashes ); tree )
  {
    if( tree->root()->hash() != util::converter::to< crypto::multihash >( trx.header().operation_merkle_root() ) )
      return std::unexpected( error_code::failure );
  }
  else
    return std::unexpected( error_code::failure );

  auto authorized = check_authority( payer );
  if( !authorized )
    return std::unexpected( authorized.error() );
  else if( !*authorized )
    return std::unexpected( error_code::authorization_failure );

  if( use_payee_nonce )
  {
    auto authorized = check_authority( payee );
    if( !authorized )
      std::unexpected( authorized.error() );
    else if( !*authorized )
      return std::unexpected( error_code::authorization_failure );
  }

  if( get_account_nonce( nonce_account ) + 1 != trx.header().nonce() )
    return std::unexpected( error_code::invalid_nonce );

  if( auto err = set_account_nonce( nonce_account, trx.header().nonce() ); err )
    return std::unexpected( err );

  if( auto err = _resource_meter.use_network_bandwidth( trx.ByteSizeLong() ); err )
    return std::unexpected( err );

  auto block_node = _state_node;

  auto err = [ & ]() -> error
  {
    auto trx_node = block_node->create_anonymous_node();
    _state_node   = trx_node;

    for( const auto& o: trx.operations() )
    {
      _op = &o;

      if( o.has_upload_contract() )
      {
        if( auto err = apply_upload_contract( o.upload_contract() ); err )
          return err;
      }
      else if( o.has_call_contract() )
      {
        if( auto err = apply_call_contract( o.call_contract() ); err )
          return err;
      }
      else
        return error( error_code::unknown_operation );
    }

    trx_node->commit();
    return {};
  }();

  protocol::transaction_receipt receipt;
  _state_node = block_node;

  if( err.is_failure() )
    return std::unexpected( err );
  if( err.is_reversion() )
  {
    receipt.set_reverted( true );
    _chronicler.push_log( "transaction reverted: " + std::string( err.message() ) );
  }

  auto used_rc = payer_session->used_rc();
  auto logs    = payer_session->logs();
  auto events  = receipt.reverted() ? std::vector< protocol::event >() : payer_session->events();

  if( !receipt.reverted() )
    for( const auto& e: payer_session->events() )
      *receipt.add_events() = e;

  for( const auto& message: payer_session->logs() )
    *receipt.add_logs() = message;

  auto end_resources = _resource_meter.remaining_resources();

  payer_session.reset();

  if( auto err = consume_account_rc( payer, used_rc ); err )
    return std::unexpected( err );

  receipt.set_id( trx.id() ), receipt.set_payer( trx.header().payer() );
  // receipt.set_payee( trx.header().payee() ); TODO: No payee field
  // receipt.set_max_payer_rc( max_payer_rc ); Is this needed without a mempool?
  receipt.set_rc_limit( trx.header().rc_limit() );
  receipt.set_rc_used( used_rc );
  receipt.set_disk_storage_used( start_resources.disk_storage - end_resources.disk_storage );
  receipt.set_network_bandwidth_used( start_resources.network_bandwidth - end_resources.network_bandwidth );
  receipt.set_compute_bandwidth_used( start_resources.compute_bandwidth - end_resources.compute_bandwidth );

  for( const auto& entry: _state_node->get_delta_entries() )
    *receipt.add_state_delta_entries() = entry;

  return receipt;
}

error execution_context::apply_upload_contract( const protocol::upload_contract_operation& op )
{
  auto authorized = check_authority(
    bytes_s( reinterpret_cast< const std::byte* >( op.contract_id().data() ),
             reinterpret_cast< const std::byte* >( op.contract_id().data() ) + op.contract_id().size() ) );
  if( !authorized )
    return authorized.error();
  else if( !*authorized )
    return error( error_code::authorization_failure );

  contract_metadata_object contract_meta;
  if( auto hash = crypto::hash( crypto::multicodec::sha2_256 ); hash )
    contract_meta.set_hash( util::converter::as< std::string >( *hash ) );
  else
    throw std::runtime_error( std::string( hash.error().message() ) );

  auto contract_meta_str = util::converter::as< std::string >( contract_meta );

  _state_node->put_object( state::space::contract_bytecode(), op.contract_id(), &op.bytecode() );
  _state_node->put_object( state::space::contract_metadata(), op.contract_id(), &contract_meta_str );

  return {};
}

error execution_context::apply_call_contract( const protocol::call_contract_operation& op )
{
  std::vector< bytes_s > args;
  args.reserve( op.args_size() );

  for( const auto& arg: op.args() )
    args.emplace_back( reinterpret_cast< const std::byte* >( arg.data() ),
                       reinterpret_cast< const std::byte* >( arg.data() ) + arg.size() );

  auto result =
    call_program( bytes_s( reinterpret_cast< const std::byte* >( op.contract_id().data() ),
                           reinterpret_cast< const std::byte* >( op.contract_id().data() ) + op.contract_id().size() ),
                  op.entry_point(),
                  args );

  if( !result )
    return result.error();

  return {};
}

uint64_t execution_context::get_account_rc( bytes_s address ) const
{
  return 1'000'000'000;
}

error execution_context::consume_account_rc( bytes_s account, uint64_t rc )
{
  return {};
}

uint64_t execution_context::get_account_nonce( bytes_s address ) const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  const auto nonce_bytes =
    _state_node->get_object( state::space::transaction_nonce(),
                             std::string( reinterpret_cast< const char* >( address.data() ), address.size() ) );
  if( nonce_bytes == nullptr )
    return 0;

  return util::converter::to< uint64_t >( *nonce_bytes );
}

error execution_context::set_account_nonce( bytes_s address, uint64_t nonce )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  auto nonce_str = util::converter::as< std::string >( nonce );

  return _resource_meter.use_disk_storage(
    _state_node->put_object( state::space::transaction_nonce(),
                             std::string( reinterpret_cast< const char* >( address.data() ), address.size() ),
                             &nonce_str ) );
}

bytes_s execution_context::get_chain_id() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  auto chain_id = _state_node->get_object( state::space::metadata(), state::key::chain_id );
  if( chain_id == nullptr )
    throw std::runtime_error( "could not find chain id" );

  return bytes_s( reinterpret_cast< const std::byte* >( chain_id->data() ),
                  reinterpret_cast< const std::byte* >( chain_id->data() ) + chain_id->size() );
}

head_info execution_context::get_head_info() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  head_info hi;
  hi.mutable_head_topology()->set_id( util::converter::as< std::string >( _state_node->id() ) );
  hi.mutable_head_topology()->set_previous( util::converter::as< std::string >( _state_node->parent_id() ) );
  hi.mutable_head_topology()->set_height( _state_node->revision() );
  hi.set_last_irreversible_block( get_last_irreversible_block() );
  hi.set_head_block_time( _state_node->block_header().timestamp() );

  return hi;
}

resource_limit_data execution_context::get_resource_limits() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  auto resource_limits_object = _state_node->get_object( state::space::metadata(), state::key::resource_limit_data );
  if( resource_limits_object == nullptr )
    throw std::runtime_error( "resource limit data does not exist" );

  return util::converter::to< resource_limit_data >( *resource_limits_object );
}

uint64_t execution_context::get_last_irreversible_block() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _state_node->revision() > default_irreversible_threshold
           ? _state_node->revision() - default_irreversible_threshold
           : 0;
}

crypto::multihash execution_context::get_state_merkle_root() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( !_state_node->is_finalized() )
    throw std::runtime_error( "state node is not final" );

  return _state_node->merkle_root();
}

resource_meter& execution_context::resource_meter()
{
  return _resource_meter;
}

chronicler& execution_context::chronicler()
{
  return _chronicler;
}

std::shared_ptr< session > execution_context::make_session( uint64_t rc )
{
  auto session = std::make_shared< chain::session >( rc );
  _resource_meter.set_session( session );
  _chronicler.set_session( session );
  return session;
}

std::expected< uint32_t, error > execution_context::contract_entry_point()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return _stack.peek_frame().entry_point;
}

std::expected< std::span< const bytes_v >, error > execution_context::contract_arguments()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return std::span( _stack.peek_frame().arguments.begin(), _stack.peek_frame().arguments.end() );
}

error execution_context::write_output( bytes_s bytes )
{
  auto& output = _stack.peek_frame().output;

  output.insert( output.end(), bytes.begin(), bytes.end() );

  return {};
}

state_db::object_space execution_context::create_object_space( uint32_t id )
{
  state_db::object_space space{ .system = false, .id = id };
  const auto& frame = _stack.peek_frame();
  std::copy( frame.contract_id.begin(), frame.contract_id.end(), space.address.begin() );

  return space;
}

std::expected< bytes_s, error > execution_context::get_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  auto result = _state_node->get_object( create_object_space( id ),
                                         std::string( reinterpret_cast< const char* >( key.data() ), key.size() ) );

  if( result )
    return bytes_s( reinterpret_cast< const std::byte* >( result->data() ),
                    reinterpret_cast< const std::byte* >( result->data() + result->size() ) );

  return bytes_s();
}

std::expected< std::pair< bytes_s, bytes_v >, error > execution_context::get_next_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  const auto [ result, next_key ] =
    _state_node->get_next_object( create_object_space( id ),
                                  std::string( reinterpret_cast< const char* >( key.data() ), key.size() ) );

  if( result )
    std::make_pair( bytes_s( reinterpret_cast< const std::byte* >( result->data() ),
                             reinterpret_cast< const std::byte* >( result->data() + result->size() ) ),
                    std::move( next_key ) );

  return make_pair( bytes_s(), bytes_v() );
}

std::expected< std::pair< bytes_s, bytes_v >, error > execution_context::get_prev_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  const auto [ result, prev_key ] =
    _state_node->get_prev_object( create_object_space( id ),
                                  std::string( reinterpret_cast< const char* >( key.data() ), key.size() ) );

  if( result )
    std::make_pair( bytes_s( reinterpret_cast< const std::byte* >( result->data() ),
                             reinterpret_cast< const std::byte* >( result->data() + result->size() ) ),
                    std::move( prev_key ) );

  return make_pair( bytes_s(), bytes_v() );
}

error execution_context::put_object( uint32_t id, bytes_s key, bytes_s value )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  std::string value_str( reinterpret_cast< const char* >( value.data() ), value.size() );

  return _resource_meter.use_disk_storage(
    _state_node->put_object( create_object_space( id ),
                             std::string( reinterpret_cast< const char* >( key.data() ), key.size() ),
                             &value_str ) );
}

error execution_context::remove_object( uint32_t id, bytes_s key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _resource_meter.use_disk_storage(
    _state_node->remove_object( create_object_space( id ),
                                std::string( reinterpret_cast< const char* >( key.data() ), key.size() ) ) );
}

std::expected< void, error > execution_context::log( bytes_s message )
{
  _chronicler.push_log( message );

  return {};
}

error execution_context::event( bytes_s name, bytes_s data, const std::vector< bytes_s >& impacted )
{
  if( name.size() == 0 )
    return error( error_code::reversion );

  if( name.size() > 128 )
    return error( error_code::reversion );
  if( !validate_utf( std::string_view( reinterpret_cast< const char* >( name.data() ), name.size() ) ) )
    return error( error_code::reversion );

  protocol::event ev;
  ev.set_source( std::string( reinterpret_cast< const char* >( _stack.peek_frame().contract_id.data() ),
                              _stack.peek_frame().contract_id.size() ) );
  ev.set_name( std::string( reinterpret_cast< const char* >( name.data() ), name.size() ) );
  ev.set_data( std::string( reinterpret_cast< const char* >( data.data() ), data.size() ) );

  for( auto& imp: impacted )
    *ev.add_impacted() = std::string( reinterpret_cast< const char* >( imp.data() ), imp.size() );

  _chronicler.push_event( _trx ? _trx->id() : std::optional< std::string >(), std::move( ev ) );

  return {};
}

std::expected< bool, error > execution_context::check_authority( bytes_s account )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( _intent == intent::read_only )
    return std::unexpected( error_code::reversion );

  const auto contract_meta_bytes =
    _state_node->get_object( state::space::contract_metadata(),
                             std::string( reinterpret_cast< const char* >( account.data() ), account.size() ) );
  // Contract case
  if( contract_meta_bytes )
  {
    std::vector< bytes_s > authorize_args;

    // Calling from within a contract
    if( _stack.size() > 0 )
    {
      const auto& frame = _stack.peek_frame();

      bytes_v entry_point_bytes( reinterpret_cast< const std::byte* >( &frame.entry_point ),
                                 reinterpret_cast< const std::byte* >( &frame.entry_point )
                                   + sizeof( frame.entry_point ) );

      authorize_args.reserve( frame.arguments.size() + 1 );
      authorize_args.emplace_back( entry_point_bytes.begin(), entry_point_bytes.end() );

      for( const auto& arg: frame.arguments )
        authorize_args.emplace_back( arg.begin(), arg.end() );
    }

    return call_program_privileged( account, authorize_entrypoint, authorize_args )
      .and_then(
        []( auto&& bytes ) -> std::expected< bool, error >
        {
          if( bytes.size() != sizeof( bool ) )
            return std::unexpected( error_code::reversion );

          return *reinterpret_cast< bool* >( bytes.data() );
        } );
  }
  // Raw address case
  else
  {
    if( _trx == nullptr )
      throw std::runtime_error( "transaction required for check authority" );

    std::size_t sig_index = 0;

    for( ; sig_index < _recovered_signatures.size(); ++sig_index )
    {
      const auto& signer_address = _recovered_signatures[ sig_index ];
      if( std::equal( signer_address.begin(), signer_address.end(), account.begin(), account.end() ) )
        return true;
    }

    for( ; sig_index < _trx->signatures_size(); _trx->signatures() )
    {
      const auto& sig    = _trx->signatures( sig_index ).signature();
      const auto& signer = _trx->signatures( sig_index ).signer();

      if( sig.size() != sizeof( crypto::signature ) )
        return std::unexpected( error_code::invalid_signature );

      crypto::public_key_data public_bytes = util::converter::as< crypto::public_key_data >( signer );
      crypto::public_key signer_key( public_bytes );
      crypto::signature signature = util::converter::as< crypto::signature >( sig );

      if( !signer_key.verify( signature, util::converter::to< crypto::multihash >( _trx->id() ) ) )
        return std::unexpected( error_code::invalid_signature );

      _recovered_signatures.emplace_back( signer_key.bytes() );

      if( std::equal( account.begin(), account.end(), public_bytes.begin(), public_bytes.end() ) )
        return true;
    }
  }

  return false;
}

std::expected< bytes_s, error > execution_context::get_caller()
{
  if( _stack.size() == 1 )
    return bytes_s();

  auto frame = _stack.pop_frame();
  bytes_s caller( _stack.peek_frame().contract_id.data(), _stack.peek_frame().contract_id.size() );
  _stack.push_frame( std::move( frame ) );

  return caller;
}

std::expected< bytes_v, error >
execution_context::call_program( bytes_s address, uint32_t entry_point, const std::vector< bytes_s >& args )
{
  if( entry_point == authorize_entrypoint )
    return std::unexpected( error_code::insufficient_privileges );

  return call_program_privileged( address, entry_point, args );
}

std::expected< bytes_v, error >
execution_context::call_program_privileged( bytes_s address, uint32_t entry_point, const std::vector< bytes_s >& args )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  std::vector< bytes_v > args_v;
  args_v.reserve( args.size() );

  for( auto& arg: args )
    args_v.emplace_back( bytes_v( arg.begin(), arg.end() ) );

  _stack.push_frame( { .contract_id = address, .arguments = std::move( args_v ), .entry_point = entry_point } );

  error err;

  std::string key( reinterpret_cast< const char* >( address.data() ), address.size() );
  if( program_registry.contains( key ) )
  {
    err = program_registry.at( key )->start( this, entry_point, args );
  }
  else
  {
    const auto contract =
      _state_node->get_object( state::space::contract_bytecode(),
                               std::string( reinterpret_cast< const char* >( address.data() ), address.size() ) );
    if( !contract )
      return std::unexpected( error_code::invalid_contract );

    const auto contract_meta_bytes =
      _state_node->get_object( state::space::contract_metadata(),
                               std::string( reinterpret_cast< const char* >( address.data() ), address.size() ) );
    if( !contract_meta_bytes )
      throw std::runtime_error( "contract metadata does not exist" );

    auto contract_meta = util::converter::to< contract_metadata_object >( *contract_meta_bytes );
    if( !contract_meta.hash().size() )
      throw std::runtime_error( "contract hash does not exist" );

    host_api hapi( *this );
    err = _vm_backend->run( hapi, *contract, contract_meta.hash() );
  }

  auto frame = _stack.pop_frame();

  if( err )
    return std::unexpected( err );

  return frame.output;
}

} // namespace koinos::chain
