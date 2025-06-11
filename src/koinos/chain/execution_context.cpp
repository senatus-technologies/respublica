#include <algorithm>
#include <stdexcept>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/locale/utf.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/crypto/crypto.hpp>
#include <koinos/util/memory.hpp>

namespace koinos::chain {

using koinos::error::error_code;

constexpr uint64_t default_account_resources       = 1'000'000'000;
constexpr uint64_t default_disk_storage_limit      = 409'600;
constexpr uint64_t default_disk_storage_cost       = 10;
constexpr uint64_t default_network_bandwidth_limit = 1'048'576;
constexpr uint64_t default_network_bandwidth_cost  = 5;
constexpr uint64_t default_compute_bandwidth_limit = 100'000'000;
constexpr uint64_t default_compute_bandwidth_cost  = 1;

constexpr auto event_name_limit = 128;

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

execution_context::execution_context( const std::shared_ptr< vm_manager::vm_backend >& vm_backend, chain::intent intent ):
    _vm_backend( vm_backend ),
    _intent( intent )
{}

void execution_context::set_state_node( const state_db::state_node_ptr& node )
{
  _state_node = node;
}

void execution_context::clear_state_node()
{
  _state_node.reset();
}

std::expected< protocol::block_receipt, error > execution_context::apply( const protocol::block& block )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  protocol::block_receipt receipt;
  _resource_meter.set_resource_limits( resource_limits() );

  auto start_resources = _resource_meter.remaining_resources();

  // Process block signature
  auto genesis_key = _state_node->get( state::space::metadata(), state::key::genesis_key() );
  if( !genesis_key )
    throw std::runtime_error( "genesis address not found" );

  if( !std::ranges::equal( *genesis_key, block.signer ) )
    return std::unexpected( error_code::invalid_signature );

  {
    std::stringstream ss;
    boost::archive::binary_oarchive oa( ss );
    oa << block;
    const auto serialized_block = ss.str();
    _state_node->put( state::space::metadata(),
                      state::key::head_block(),
                      std::as_bytes( std::span< const char >( serialized_block ) ) );
  }

  for( const auto& trx: block.transactions )
  {
    auto trx_receipt = apply( trx );

    if( trx_receipt )
      receipt.transaction_receipts.emplace_back( trx_receipt.value() );
    else if( trx_receipt.error().is_failure() )
      return std::unexpected( trx_receipt.error() );
  }

  const auto& rld                   = _resource_meter.resource_limits();
  const auto& system_resources      = _resource_meter.system_resources();
  const auto& end_charged_resources = _resource_meter.remaining_resources();

  [[maybe_unused]]
  uint64_t system_rc_cost = system_resources.disk_storage * rld.disk_storage_cost
                            + system_resources.network_bandwidth * rld.network_bandwidth_cost
                            + system_resources.compute_bandwidth * rld.compute_bandwidth_cost;
  // Consume RC is currently empty

  // Consume block resources is currently empty

  const auto& end_resources = _resource_meter.remaining_resources();

  receipt.id                        = block.id;
  receipt.height                    = block.height;
  receipt.disk_storage_used         = start_resources.disk_storage - end_resources.disk_storage;
  receipt.network_bandwidth_used    = start_resources.network_bandwidth - end_resources.network_bandwidth;
  receipt.compute_bandwidth_used    = start_resources.compute_bandwidth - end_resources.compute_bandwidth;
  receipt.disk_storage_charged      = start_resources.disk_storage - end_charged_resources.disk_storage;
  receipt.network_bandwidth_charged = start_resources.network_bandwidth - end_charged_resources.network_bandwidth;
  receipt.compute_bandwidth_charged = start_resources.compute_bandwidth - end_charged_resources.compute_bandwidth;

  for( const auto& [ transaction_id, event ]: _chronicler.events() )
    if( !transaction_id )
      receipt.events.push_back( event );

  receipt.logs = _chronicler.logs();

  return receipt;
}

std::expected< protocol::transaction_receipt, error >
execution_context::apply( const protocol::transaction& transaction )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _trx = &transaction;
  _recovered_signatures.clear();

  bool use_payee_nonce = std::any_of( transaction.payee.begin(),
                                      transaction.payee.end(),
                                      []( std::byte elem )
                                      {
                                        return elem != std::byte{ 0x00 };
                                      } );

  const auto& nonce_account = use_payee_nonce ? transaction.payee : transaction.payer;

  const auto& start_resources = _resource_meter.remaining_resources();

  auto payer_session   = make_session( transaction.resource_limit );
  auto payer_resources = account_resources( transaction.payer );

  if( payer_resources < transaction.resource_limit )
    return std::unexpected( error_code::failure );

  auto authorized = check_authority( transaction.payer );
  if( !authorized )
    return std::unexpected( authorized.error() );
  else if( !*authorized )
    return std::unexpected( error_code::authorization_failure );

  if( use_payee_nonce )
  {
    auto authorized = check_authority( transaction.payee );

    if( !authorized )
      std::unexpected( authorized.error() );
    else if( !*authorized )
      return std::unexpected( error_code::authorization_failure );
  }

  if( account_nonce( nonce_account ) + 1 != transaction.nonce )
    return std::unexpected( error_code::invalid_nonce );

  if( auto err = set_account_nonce( nonce_account, transaction.nonce ); err )
    return std::unexpected( err );

  if( auto err = _resource_meter.use_network_bandwidth( transaction.size() ); err )
    return std::unexpected( err );

  auto block_node = _state_node;

  auto err = [ & ]() -> error
  {
    auto trx_node = block_node->make_child();
    _state_node   = trx_node;

    for( const auto& o: transaction.operations )
    {
      _op = &o;

      if( std::holds_alternative< protocol::upload_program >( o ) )
      {
        if( auto err = apply( std::get< protocol::upload_program >( o ) ); err )
          return err;
      }
      else if( std::holds_alternative< protocol::call_program >( o ) )
      {
        if( auto err = apply( std::get< protocol::call_program >( o ) ); err )
          return err;
      }
      else [[unlikely]]
        return error( error_code::unknown_operation );
    }

    trx_node->squash();
    return {};
  }();

  protocol::transaction_receipt receipt;
  _state_node = block_node;

  if( err.is_failure() )
    return std::unexpected( err );

  if( err.is_reversion() )
  {
    receipt.reverted = true;
    _chronicler.push_log( "transaction reverted: " + std::string( err.message() ) );
  }

  auto used_rc = payer_session->used_rc();
  auto logs    = payer_session->logs();
  auto events  = receipt.reverted ? std::vector< protocol::event >() : payer_session->events();

  if( !receipt.reverted )
    receipt.events = payer_session->events();

  receipt.logs = payer_session->logs();

  auto end_resources = _resource_meter.remaining_resources();

  payer_session.reset();

  if( auto err = consume_account_resources( transaction.payer, used_rc ); err )
    return std::unexpected( err );

  receipt.id                     = transaction.id;
  receipt.payer                  = transaction.payer;
  receipt.payee                  = transaction.payee;
  receipt.resource_limit         = transaction.resource_limit;
  receipt.resource_used          = used_rc;
  receipt.disk_storage_used      = start_resources.disk_storage - end_resources.disk_storage;
  receipt.network_bandwidth_used = start_resources.network_bandwidth - end_resources.network_bandwidth;
  receipt.compute_bandwidth_used = start_resources.compute_bandwidth - end_resources.compute_bandwidth;

  return receipt;
}

error execution_context::apply( const protocol::upload_program& op )
{
  auto authorized = check_authority( op.id );

  if( !authorized )
    return authorized.error();
  else if( !*authorized )
    return error( error_code::authorization_failure );

  std::span< const std::byte > key( op.id );
  std::span< const std::byte > value( op.bytecode );
  auto hash = crypto::hash( op.bytecode );

  _state_node->put( state::space::program_bytecode(), key, value );
  _state_node->put( state::space::program_metadata(), key, std::span< const std::byte >( hash ) );

  return {};
}

error execution_context::apply( const protocol::call_program& op )
{
  std::vector< std::span< const std::byte > > args;
  args.reserve( op.arguments.size() );
  for( const auto& arg: op.arguments )
    args.emplace_back( std::span( arg ) );

  auto result = call_program( op.id, op.entry_point, args );

  if( !result )
    return result.error();

  return {};
}

uint64_t execution_context::account_resources( const protocol::account& account ) const
{
  return default_account_resources;
}

error execution_context::consume_account_resources( const protocol::account& account, uint64_t rc )
{
  return {};
}

uint64_t execution_context::account_nonce( const protocol::account& account ) const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto nonce_bytes = _state_node->get( state::space::transaction_nonce(), account ); nonce_bytes )
    return *util::start_lifetime_as< const uint64_t >( nonce_bytes->data() );

  return 0;
}

error execution_context::set_account_nonce( const protocol::account& account, uint64_t nonce )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _resource_meter.use_disk_storage(
    _state_node->put( state::space::transaction_nonce(),
                      account,
                      std::as_bytes( std::span< std::uint64_t >( &nonce, 1 ) ) ) );
}

const crypto::digest& execution_context::network_id() const noexcept
{
  static const auto id = crypto::hash( "celeritas" );
  return id;
}

state::head execution_context::head() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  auto state_node = std::dynamic_pointer_cast< state_db::permanent_state_node >( _state_node );
  if( !state_node )
    throw std::runtime_error( "head state node unexpectedly temporary" );

  return state::head{ .id                      = state_node->id(),
                      .height                  = state_node->revision(),
                      .previous                = state_node->parent_id(),
                      .last_irreversible_block = last_irreversible_block(),
                      .state_merkle_root       = state_node->merkle_root() };
}

const state::resource_limits& execution_context::resource_limits() const
{
  static state::resource_limits limits{ .disk_storage_limit      = default_disk_storage_limit,
                                        .disk_storage_cost       = default_disk_storage_cost,
                                        .network_bandwidth_limit = default_network_bandwidth_limit,
                                        .network_bandwidth_cost  = default_network_bandwidth_cost,
                                        .compute_bandwidth_limit = default_compute_bandwidth_limit,
                                        .compute_bandwidth_cost  = default_compute_bandwidth_cost };

  return limits;
}

uint64_t execution_context::last_irreversible_block() const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _state_node->revision() > default_irreversible_threshold
           ? _state_node->revision() - default_irreversible_threshold
           : 0;
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

std::span< const std::span< const std::byte > > execution_context::program_arguments()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return _stack.peek_frame().arguments;
}

error execution_context::write_output( std::span< const std::byte > bytes )
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

std::expected< std::span< const std::byte >, error > execution_context::get_object( uint32_t id,
                                                                                    std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto result = _state_node->get( create_object_space( id ), key ); result )
    return *result;

  return {};
}

std::expected< std::pair< std::span< const std::byte >, std::span< const std::byte > >, error >
execution_context::get_next_object( uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto result = _state_node->next( create_object_space( id ), key ); result )
    return *result;

  return make_pair( std::span< const std::byte >(), std::vector< std::byte >() );
}

std::expected< std::pair< std::span< const std::byte >, std::span< const std::byte > >, error >
execution_context::get_prev_object( uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto result = _state_node->previous( create_object_space( id ), key ); result )
    return *result;

  return make_pair( std::span< const std::byte >(), std::vector< std::byte >() );
}

error execution_context::put_object( uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _resource_meter.use_disk_storage( _state_node->put( create_object_space( id ), key, value ) );
}

error execution_context::remove_object( uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _resource_meter.use_disk_storage( _state_node->remove( create_object_space( id ), key ) );
}

std::expected< void, error > execution_context::log( std::span< const std::byte > message )
{
  _chronicler.push_log( message );

  return {};
}

error execution_context::event( std::span< const std::byte > name,
                                std::span< const std::byte > data,
                                const std::vector< std::span< const std::byte > >& impacted )
{
  if( name.size() == 0 )
    return error( error_code::reversion );

  if( name.size() > event_name_limit )
    return error( error_code::reversion );
  if( !validate_utf( std::string_view( util::pointer_cast< const char* >( name.data() ), name.size() ) ) )
    return error( error_code::reversion );

  protocol::event ev;

  assert( _stack.peek_frame().contract_id.size() <= ev.source.size() );
  std::copy( _stack.peek_frame().contract_id.begin(), _stack.peek_frame().contract_id.end(), ev.source.begin() );

  ev.name = std::string( util::pointer_cast< const char* >( name.data() ), name.size() );
  ev.data = std::vector( data.begin(), data.end() );

  for( const auto& imp: impacted )
  {
    if( imp.size() > sizeof( protocol::account ) )
      return error( error_code::reversion );
    ev.impacted.emplace_back();
    std::copy( imp.begin(), imp.end(), ev.impacted.back().begin() );
  }

  _chronicler.push_event( _trx ? _trx->id : std::optional< crypto::digest >(), std::move( ev ) );

  return {};
}

std::expected< bool, error > execution_context::check_authority( const protocol::account& account )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( _intent == intent::read_only )
    return std::unexpected( error_code::reversion );

  if( auto contract_meta_bytes = _state_node->get( state::space::program_metadata(), account ); contract_meta_bytes )
  {
    return call_program_privileged( account,
                                    authorize_entrypoint,
                                    _stack.size() > 0 ? _stack.peek_frame().arguments
                                                      : std::span< const std::span< const std::byte > >() )
      .and_then(
        []( auto&& bytes ) -> std::expected< bool, error >
        {
          if( bytes.size() != sizeof( bool ) )
            return std::unexpected( error_code::reversion );

          return *util::start_lifetime_as< const bool >( bytes.data() );
        } );
  }
  else
  {
    // Raw address case
    if( _trx == nullptr )
      throw std::runtime_error( "transaction required for check authority" );

    std::size_t sig_index = 0;

    for( ; sig_index < _recovered_signatures.size(); ++sig_index )
    {
      const auto& signer_address = _recovered_signatures[ sig_index ];
      if( std::equal( signer_address.begin(), signer_address.end(), account.begin(), account.end() ) )
        return true;
    }

    for( ; sig_index < _trx->authorizations.size(); ++sig_index )
    {
      const auto& signature = _trx->authorizations[ sig_index ].signature;
      const auto& signer    = _trx->authorizations[ sig_index ].signer;

      crypto::public_key signer_key( signer );

      if( !signer_key.verify( signature, _trx->id ) )
        return std::unexpected( error_code::invalid_signature );

      _recovered_signatures.emplace_back( signer );

      if( std::equal( account.begin(), account.end(), signer.begin(), signer.end() ) )
        return true;
    }
  }

  return false;
}

std::expected< std::span< const std::byte >, error > execution_context::get_caller()
{
  if( _stack.size() == 1 )
    return {};

  auto frame = _stack.pop_frame();
  std::span< const std::byte > caller( _stack.peek_frame().contract_id.data(), _stack.peek_frame().contract_id.size() );
  _stack.push_frame( std::move( frame ) );

  return caller;
}

std::expected< std::vector< std::byte >, error >
execution_context::call_program( const protocol::account& account,
                                 uint32_t entry_point,
                                 const std::vector< std::span< const std::byte > >& args )
{
  if( entry_point == authorize_entrypoint )
    return std::unexpected( error_code::insufficient_privileges );

  return call_program_privileged( account, entry_point, args );
}

std::expected< std::vector< std::byte >, error >
execution_context::call_program_privileged( const protocol::account& account,
                                            uint32_t entry_point,
                                            std::span< const std::span< const std::byte > > args )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _stack.push_frame( { .contract_id = account, .arguments = args, .entry_point = entry_point } );

  error err;

  if( auto registry_iterator = program_registry.find( account ); registry_iterator != program_registry.end() )
  {
    err = registry_iterator->second->start( this, entry_point, args );
  }
  else
  {
    auto contract = _state_node->get( state::space::program_bytecode(), account );

    if( !contract )
      return std::unexpected( error_code::invalid_contract );

    auto contract_meta = _state_node->get( state::space::program_metadata(), account );

    if( !contract_meta )
      throw std::runtime_error( "contract metadata does not exist" );

    if( !contract_meta->size() )
      throw std::runtime_error( "contract hash does not exist" );

    host_api hapi( *this );
    err = _vm_backend->run( hapi, *contract, *contract_meta );
  }

  auto frame = _stack.pop_frame();

  if( err )
    return std::unexpected( err );

  return frame.output;
}

} // namespace koinos::chain
