#include <algorithm>
#include <expected>
#include <ranges>
#include <stdexcept>
#include <string>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/endian.hpp>
#include <boost/locale/utf.hpp>

#include <koinos/controller/execution_context.hpp>
#include <koinos/controller/host_api.hpp>
#include <koinos/controller/state.hpp>
#include <koinos/crypto.hpp>
#include <koinos/memory.hpp>

#include <koinos/encode.hpp>

namespace koinos::controller {

const program_registry_map execution_context::program_registry = []()
{
  program_registry_map registry;
  registry.emplace( protocol::system_program( "coin" ), std::make_unique< program::coin >() );
  return registry;
}();

const program_registry_map_view execution_context::program_registry_view = []()
{
  program_registry_map_view registry;

  for( auto itr = program_registry.begin(); itr != execution_context::program_registry.end(); ++itr )
    registry.emplace( std::span< const std::byte, std::dynamic_extent >( itr->first ), itr );

  return registry;
}();

constexpr std::uint64_t default_account_resources       = 1'000'000'000;
constexpr std::uint64_t default_disk_storage_limit      = 409'600;
constexpr std::uint64_t default_disk_storage_cost       = 10;
constexpr std::uint64_t default_network_bandwidth_limit = 1'048'576;
constexpr std::uint64_t default_network_bandwidth_cost  = 5;
constexpr std::uint64_t default_compute_bandwidth_limit = 100'000'000;
constexpr std::uint64_t default_compute_bandwidth_cost  = 1;

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

execution_context::execution_context( const std::shared_ptr< vm::virtual_machine >& vm, controller::intent intent ):
    _vm( vm ),
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

result< protocol::block_receipt > execution_context::apply( const protocol::block& block )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  protocol::block_receipt receipt;
  _resource_meter.set_resource_limits( resource_limits() );

  auto start_resources = _resource_meter.remaining_resources();

  auto genesis_key = _state_node->get( state::space::metadata(), state::key::genesis_key() );
  if( !genesis_key )
    throw std::runtime_error( "genesis address not found" );

  if( !std::ranges::equal( *genesis_key, crypto::public_key( block.signer ).bytes() ) )
    return std::unexpected( controller_errc::invalid_signature );

  for( const auto& transaction: block.transactions )
  {
    auto transaction_receipt = apply( transaction );

    if( transaction_receipt )
      receipt.transaction_receipts.emplace_back( transaction_receipt.value() );
    else if( transaction_receipt.error().category() != reversion_category() )
      return std::unexpected( transaction_receipt.error() );
  }

  const auto& limits                = _resource_meter.resource_limits();
  const auto& system_resources      = _resource_meter.system_resources();
  const auto& end_charged_resources = _resource_meter.remaining_resources();

  [[maybe_unused]]
  std::uint64_t system_rc_cost = system_resources.disk_storage * limits.disk_storage_cost
                                 + system_resources.network_bandwidth * limits.network_bandwidth_cost
                                 + system_resources.compute_bandwidth * limits.compute_bandwidth_cost;
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

result< protocol::transaction_receipt > execution_context::apply( const protocol::transaction& transaction )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  _transaction = &transaction;
  _verified_signatures.clear();

  bool use_payee_nonce = std::any_of( transaction.payee.begin(),
                                      transaction.payee.end(),
                                      []( std::byte elem )
                                      {
                                        return elem != std::byte{ 0x00 };
                                      } );

  const auto& nonce_account = use_payee_nonce ? transaction.payee : transaction.payer;

  const auto& initial_resources = _resource_meter.remaining_resources();

  auto payer_session   = make_session( transaction.resource_limit );
  auto payer_resources = account_resources( transaction.payer );

  if( payer_resources < transaction.resource_limit )
    return std::unexpected( controller_errc::insufficient_resources );

  if( auto authorized = check_authority( transaction.payer ); authorized )
  {
    if( !authorized.value() )
      return std::unexpected( controller_errc::authorization_failure );
  }
  else
  {
    return std::unexpected( authorized.error() );
  }

  if( use_payee_nonce )
  {
    if( auto authorized = check_authority( transaction.payee ); authorized )
    {
      if( !authorized.value() )
        return std::unexpected( controller_errc::authorization_failure );
    }
    else
    {
      return std::unexpected( authorized.error() );
    }
  }

  if( account_nonce( nonce_account ) + 1 != transaction.nonce )
    return std::unexpected( controller_errc::invalid_nonce );

  if( auto error = set_account_nonce( nonce_account, transaction.nonce ); error )
    return std::unexpected( error );

  if( auto error = _resource_meter.use_network_bandwidth( transaction.size() ); error )
    return std::unexpected( error );

  auto block_node = _state_node;

  auto error = [ & ]() -> std::error_code
  {
    auto transaction_node = block_node->make_child();
    _state_node           = transaction_node;

    for( const auto& o: transaction.operations )
    {
      _operation = &o;

      if( std::holds_alternative< protocol::upload_program >( o ) )
      {
        if( auto error = apply( std::get< protocol::upload_program >( o ) ); error )
          return error;
      }
      else if( std::holds_alternative< protocol::call_program >( o ) )
      {
        if( auto error = apply( std::get< protocol::call_program >( o ) ); error )
          return error;
      }
      else [[unlikely]]
      {
        return reversion_errc::unknown_operation;
      }
    }

    transaction_node->squash();
    return controller_errc::ok;
  }();

  _state_node = block_node;

  protocol::transaction_receipt receipt;

  if( error )
  {
    if( error.category() == reversion_category() )
    {
      receipt.reverted = true;
      _chronicler.push_log( "transaction reverted: " + error.message() );
    }
    else
    {
      return std::unexpected( error );
    }
  }

  auto used_resources = payer_session->used_resources();
  auto logs           = payer_session->logs();
  auto events         = receipt.reverted ? std::vector< protocol::event >() : payer_session->events();

  if( !receipt.reverted )
    receipt.events = payer_session->events();

  receipt.logs = payer_session->logs();

  auto remaining_resources = _resource_meter.remaining_resources();

  payer_session.reset();

  if( auto error = consume_account_resources( transaction.payer, used_resources ); error )
    return std::unexpected( error );

  receipt.id                     = transaction.id;
  receipt.payer                  = transaction.payer;
  receipt.payee                  = transaction.payee;
  receipt.resource_limit         = transaction.resource_limit;
  receipt.resource_used          = used_resources;
  receipt.disk_storage_used      = initial_resources.disk_storage - remaining_resources.disk_storage;
  receipt.network_bandwidth_used = initial_resources.network_bandwidth - remaining_resources.network_bandwidth;
  receipt.compute_bandwidth_used = initial_resources.compute_bandwidth - remaining_resources.compute_bandwidth;

  return receipt;
}

std::error_code execution_context::apply( const protocol::upload_program& op )
{
  result< bool > authorized;

  /*
   * The first upload must be signed with the user key associated with the
   * program id. Subsequent uploads must be authorized by the program itself.
   *
   * If the program exists, check its authority, otherwise check the user
   * account associated with the program.
   */
  if( _state_node->get( state::space::program_data(), memory::as_bytes( op.id ) ) )
    authorized = check_authority( op.id );
  else
    authorized = check_authority( protocol::user_account( op.id ) );

  if( authorized )
  {
    if( !authorized.value() )
      return controller_errc::authorization_failure;
  }
  else
  {
    return authorized.error();
  }

#pragma message( "C++26 TODO: Replace with std::ranges::concat" )
  _state_node->put( state::space::program_data(),
                    memory::as_bytes( op.id ),
                    std::ranges::join_view(
                      std::array< std::span< const std::byte >, 2 >{ memory::as_bytes( crypto::hash( op.bytecode ) ),
                                                                     memory::as_bytes( op.bytecode ) } ) );

  return controller_errc::ok;
}

std::error_code execution_context::apply( const protocol::call_program& op )
{
  auto result = call_program( op.id, op.input.stdin, op.input.arguments );

  if( !result )
    return result.error();

  return controller_errc::ok;
}

std::uint64_t execution_context::account_resources( protocol::account_view account ) const
{
  return default_account_resources;
}

std::error_code execution_context::consume_account_resources( protocol::account_view account, std::uint64_t resources )
{
  return controller_errc::ok;
}

std::uint64_t execution_context::account_nonce( protocol::account_view account ) const
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto nonce_bytes = _state_node->get( state::space::transaction_nonce(), account ); nonce_bytes )
  {
    auto nonce = memory::bit_cast< std::uint64_t >( *nonce_bytes );
    boost::endian::little_to_native_inplace( nonce );
    return nonce;
  }

  return 0;
}

std::error_code execution_context::set_account_nonce( protocol::account_view account, std::uint64_t nonce )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  boost::endian::native_to_little_inplace( nonce );

  return _resource_meter.use_disk_storage(
    _state_node->put( state::space::transaction_nonce(), account, memory::as_bytes( nonce ) ) );
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

  return state::head{ .id                = state_node->id(),
                      .height            = state_node->revision(),
                      .previous          = state_node->parent_id(),
                      .state_merkle_root = state_node->merkle_root() };
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

resource_meter& execution_context::resource_meter()
{
  return _resource_meter;
}

chronicler& execution_context::chronicler()
{
  return _chronicler;
}

std::shared_ptr< session > execution_context::make_session( std::uint64_t resources )
{
  auto session = std::make_shared< controller::session >( resources );
  _resource_meter.set_session( session );
  _chronicler.set_session( session );
  return session;
}

std::span< const std::string > execution_context::arguments()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return _stack.peek_frame().arguments;
}

std::error_code execution_context::write( program::file_descriptor fd, std::span< const std::byte > buffer )
{
  if( fd == program::file_descriptor::stdout )
  {
    auto& output = _stack.peek_frame().stdout;
    output.insert( output.end(), buffer.begin(), buffer.end() );
    return reversion_errc::ok;
  }
  else if( fd == program::file_descriptor::stderr )
  {
    auto& error = _stack.peek_frame().stderr;
    error.insert( error.end(), buffer.begin(), buffer.end() );
    return reversion_errc::ok;
  }

  return reversion_errc::bad_file_descriptor;
}

std::error_code execution_context::read( program::file_descriptor fd, std::span< std::byte > buffer )
{
  if( fd == program::file_descriptor::stdin )
  {
    auto& frame        = _stack.peek_frame();
    std::size_t length = std::min( buffer.size(), frame.stdin.size() - frame.input_offset );

    std::ranges::copy( frame.stdin.data() + frame.input_offset,
                       frame.stdin.data() + frame.input_offset + length,
                       buffer.data() );
    frame.input_offset += length;
    return reversion_errc::ok;
  }

  return reversion_errc::bad_file_descriptor;
}

state_db::object_space execution_context::create_object_space( std::uint32_t id )
{
  state_db::object_space space{ .system = false, .id = id };
  const auto& frame = _stack.peek_frame();
  std::ranges::copy( frame.program_id, space.address.begin() );

  return space;
}

std::span< const std::byte > execution_context::get_object( std::uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto result = _state_node->get( create_object_space( id ), key ); result )
    return *result;

  return std::span< const std::byte >{};
}

std::pair< std::span< const std::byte >, std::span< const std::byte > >
execution_context::get_next_object( std::uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto result = _state_node->next( create_object_space( id ), key ); result )
    return *result;

  return std::make_pair( std::span< const std::byte >(), std::vector< std::byte >() );
}

std::pair< std::span< const std::byte >, std::span< const std::byte > >
execution_context::get_prev_object( std::uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( auto result = _state_node->previous( create_object_space( id ), key ); result )
    return *result;

  return std::make_pair( std::span< const std::byte >(), std::vector< std::byte >() );
}

std::error_code
execution_context::put_object( std::uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _resource_meter.use_disk_storage( _state_node->put( create_object_space( id ), key, value ) );
}

std::error_code execution_context::remove_object( std::uint32_t id, std::span< const std::byte > key )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  return _resource_meter.use_disk_storage( _state_node->remove( create_object_space( id ), key ) );
}

void execution_context::log( std::span< const std::byte > message )
{
  _chronicler.push_log( message );
}

std::error_code execution_context::event( std::span< const std::byte > name,
                                          std::span< const std::byte > data,
                                          const std::vector< std::span< const std::byte > >& impacted )
{
  if( name.size() == 0 )
    return reversion_errc::invalid_event_name;

  if( name.size() > event_name_limit )
    return reversion_errc::invalid_event_name;

  if( !validate_utf( std::string_view( memory::pointer_cast< const char* >( name.data() ), name.size() ) ) )
    return reversion_errc::invalid_event_name;

  protocol::event event;

  assert( _stack.peek_frame().program_id.size() <= event.source.size() );
  std::ranges::copy( _stack.peek_frame().program_id, event.source.begin() );

  event.name = std::string( memory::pointer_cast< const char* >( name.data() ), name.size() );
  event.data = std::vector( data.begin(), data.end() );

  for( const auto& imp: impacted )
  {
    if( imp.size() > sizeof( protocol::account ) )
      return reversion_errc::invalid_account;

    event.impacted.emplace_back();
    std::ranges::copy( imp, event.impacted.back().begin() );
  }

  _chronicler.push_event( _transaction ? _transaction->id : std::optional< crypto::digest >(), std::move( event ) );

  return controller_errc::ok;
}

result< bool > execution_context::check_authority( protocol::account_view account )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( _intent == intent::read_only )
    return std::unexpected( reversion_errc::read_only_context );

  if( account.program() )
  {
#pragma message( "When there is compiler support for constexpr std::vector, use that feature to handle this" )
    std::vector< std::byte > input;
    static constexpr std::uint32_t authorize_entry_point = 0x4a2dbd90;
    auto byte_view                                       = memory::as_bytes( authorize_entry_point );
    input.insert( input.end(), byte_view.begin(), byte_view.end() );

    return call_program( account, input )
      .and_then(
        []( auto&& output ) -> result< bool >
        {
          if( output.stdout.size() != sizeof( bool ) )
            return std::unexpected( reversion_errc::failure );

          return memory::bit_cast< bool >( output.stdout );
        } );
  }
  else
  {
    // User account case
    if( _transaction == nullptr )
      throw std::runtime_error( "transaction required for check authority" );

    std::size_t sig_index = 0;

    for( ; sig_index < _verified_signatures.size(); ++sig_index )
    {
      const auto& signer_address = _verified_signatures[ sig_index ];
      if( std::ranges::equal( signer_address, account ) )
        return true;
    }

    for( ; sig_index < _transaction->authorizations.size(); ++sig_index )
    {
      const auto& signature = _transaction->authorizations[ sig_index ].signature;
      const auto& signer    = _transaction->authorizations[ sig_index ].signer;

      if( !crypto::public_key( signer ).verify( signature, _transaction->id ) )
        return std::unexpected( controller_errc::invalid_signature );

      _verified_signatures.emplace_back( signer );

      if( std::ranges::equal( account, signer ) )
        return true;
    }
  }

  return false;
}

std::span< const std::byte > execution_context::get_caller()
{
  if( _stack.size() == 1 )
    return std::span< const std::byte >{};

  return _stack.peek_frame().program_id;
}

result< protocol::program_output > execution_context::call_program( protocol::account_view account,
                                                                    std::span< const std::byte > stdin,
                                                                    std::span< const std::string > arguments )
{
  if( !_state_node )
    throw std::runtime_error( "state node does not exist" );

  if( !account.program() )
    return std::unexpected( reversion_errc::invalid_program );

  _stack.push_frame( { .program_id = account, .arguments = arguments, .stdin = stdin } );
  std::error_code error;

  if( auto registry_iterator = program_registry_view.find( account ); registry_iterator != program_registry_view.end() )
  {
    error = registry_iterator->second->second->run( this, arguments );
  }
  else
  {
    auto program_data = _state_node->get( state::space::program_data(), account );

    if( !program_data )
      return std::unexpected( reversion_errc::invalid_program );

    if( program_data->size() < sizeof( crypto::digest ) )
      throw std::runtime_error( "contract hash does not exist" );

    host_api hapi( *this );
    error = _vm->run( hapi,
                      program_data->subspan( sizeof( crypto::digest ) ),
                      program_data->subspan( 0, sizeof( crypto::digest ) ) );
  }

  if( error )
    return std::unexpected( reversion_errc::failure );

  auto frame = _stack.pop_frame();

  return protocol::program_output{ .stdout = std::move( frame.stdout ), .stderr = std::move( frame.stderr ) };
}

} // namespace koinos::controller
