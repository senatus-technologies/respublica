#include <respublica/controller/controller.hpp>
#include <respublica/controller/execution_context.hpp>
#include <respublica/controller/host_api.hpp>
#include <respublica/controller/state.hpp>

#include <respublica/encode.hpp>
#include <respublica/log.hpp>

#include <chrono>
#include <memory>
#include <span>

namespace respublica::controller {

controller::controller( std::uint64_t read_compute_bandwidth_limit ):
    _read_compute_bandwidth_limit( read_compute_bandwidth_limit )
{
  _vm = std::make_shared< vm::virtual_machine >();
}

controller::~controller()
{
  close();
}

void controller::open( const std::filesystem::path& p,
                       const state::genesis_data& data,
                       state_db::fork_resolution_algorithm algo,
                       bool reset )
{
  _db.open(
    [ & ]( state_db::state_node_ptr& root )
    {
      for( const auto& entry: data )
      {
        if( root->get( entry.space, entry.key ) )
          throw std::runtime_error( "encountered unexpected object in initial state" );

        root->put( entry.space, entry.key, entry.value );
      }
      LOG_INFO( respublica::log::instance(), "Wrote {} genesis objects into new database", data.size() );

      if( !root->get( state::space::metadata(), state::key::genesis_key() ) )
        throw std::runtime_error( "could not find genesis public key in database" );
    },
    algo );

  if( reset )
  {
    LOG_INFO( respublica::log::instance(), "Resetting database..." );
    _db.reset();
  }

  auto head = _db.head();
  LOG_INFO( respublica::log::instance(),
            "Opened database at block - Height: {}, ID: {}",
            head->revision(),
            respublica::log::hex{ head->id().data(), head->id().size() } );
}

void controller::close()
{
  _db.close();
}

result< protocol::block_receipt > controller::process( const protocol::block& block,
                                                       std::uint64_t index_to,
                                                       std::chrono::system_clock::time_point current_time )
{
  if( !block.validate() )
    return std::unexpected( controller_errc::malformed_block );

  static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );
  static constexpr std::chrono::seconds live_delta = std::chrono::seconds( 60 );
  static constexpr state_db::state_node_id zero_id{};

  auto time_lower_bound = std::uint64_t( 0 );
  auto time_upper_bound =
    std::chrono::duration_cast< std::chrono::milliseconds >( ( current_time + time_delta ).time_since_epoch() ).count();

  const auto& block_id  = block.id;
  auto block_height     = block.height;
  const auto& parent_id = block.previous;
  auto block_node       = _db.get( block_id );
  auto parent_node      = _db.get( parent_id );

  if( block_node )
    return std::unexpected( controller_errc::ok ); // Block has been applied

  // This prevents returning "unknown previous block" when the pushed block is the LIB
  if( !parent_node )
  {
    auto root = _db.root();
    if( block_height < root->revision() )
      return std::unexpected( controller_errc::pre_irreversibility_block );

    if( block_id != root->id() )
      return std::unexpected( controller_errc::unknown_previous_block );

    return std::unexpected( controller_errc::ok ); // Block is current LIB
  }
  else if( !parent_node->final() )
    return std::unexpected( controller_errc::unknown_previous_block );

  bool live = block.timestamp > std::chrono::duration_cast< std::chrono::milliseconds >(
                                  ( current_time - live_delta ).time_since_epoch() )
                                  .count();

  if( !index_to && live )
    LOG_DEBUG( respublica::log::instance(),
               "Pushing block - Height: {}, ID: {}",
               block_height,
               respublica::log::hex{ block_id.data(), block_id.size() } );

  block_node = parent_node->make_child( block_id );

  if( !block_node )
    return std::unexpected( controller_errc::block_state_error );

  if( parent_id == zero_id )
  {
    if( block_height != 1 )
      return std::unexpected( controller_errc::unexpected_height );
  }
  else
  {
    if( block.state_merkle_root != parent_node->merkle_root() )
      return std::unexpected( controller_errc::state_merkle_mismatch );

    execution_context parent_context( _vm );

    parent_context.set_state_node( parent_node );
    auto parent_info = parent_context.head();
    time_lower_bound = parent_info.time;

    if( block_height != parent_info.height + 1 )
      return std::unexpected( controller_errc::unexpected_height );
  }

  if( ( block.timestamp > time_upper_bound ) || ( block.timestamp <= time_lower_bound ) )
    return std::unexpected( controller_errc::timestamp_out_of_bounds );

  execution_context context( _vm, intent::block_application );
  context.set_state_node( block_node );

  return context.apply( block ).and_then(
    [ & ]( auto&& receipt ) -> result< protocol::block_receipt >
    {
      if( !index_to && live )
      {
        LOG_INFO( respublica::log::instance(),
                  "Block applied - Height: {}, ID: {} [{} transaction(s)]",
                  block_height,
                  respublica::log::hex{ block_id.data(), block_id.size() },
                  block.transactions.size() );
      }
      else
      {
        if( index_to )
        {
          LOG_INFO_LIMIT( std::chrono::minutes{ 1 },
                          respublica::log::instance(),
                          "Indexing {}% - Height: {}, ID: {}",
                          respublica::log::percent{ block_height, index_to },
                          block_height,
                          respublica::log::hex{ block_id.data(), block_id.size() } );
        }
        else
        {
          LOG_INFO_LIMIT(
            std::chrono::minutes{ 1 },
            respublica::log::instance(),
            "Sync progress - Height: {}, ID: {} ({} block time remaining)",
            block_height,
            respublica::log::hex{ block_id.data(), block_id.size() },
            respublica::log::time_remaining{ current_time, std::chrono::milliseconds( block.timestamp ) } );
        }
      }

      constexpr auto default_irreversible_threshold = 60;

      auto irreversible_block = block_node->revision() > default_irreversible_threshold
                                  ? block_node->revision() - default_irreversible_threshold
                                  : 0;

      block_node->finalize();
      receipt.state_merkle_root = block_node->merkle_root();

      if( irreversible_block > _db.root()->revision() )
        _db.at_revision( irreversible_block, block_id )->commit();

      return receipt;
    } );
}

result< protocol::transaction_receipt > controller::process( const protocol::transaction& transaction, bool broadcast )
{
  if( !transaction.validate() )
    return std::unexpected( controller_errc::malformed_transaction );

  LOG_DEBUG( respublica::log::instance(),
             "Pushing transaction - ID: {}",
             respublica::log::hex{ transaction.id.data(), transaction.id.size() } );

  if( network_id() != transaction.network_id )
    return std::unexpected( controller_errc::network_id_mismatch );

  state_db::state_node_ptr head = _db.head();

  execution_context context( _vm, intent::transaction_application );
  context.set_state_node( head->make_child() );
  context.resource_meter().set_resource_limits( context.resource_limits() );

  return context.apply( transaction )
    .and_then(
      [ & ]( auto&& receipt ) -> result< protocol::transaction_receipt >
      {
        LOG_DEBUG( respublica::log::instance(),
                   "Transaction applied - ID: {}",
                   respublica::log::hex{ transaction.id.data(), transaction.id.size() } );
        return receipt;
      } );
}

const crypto::digest& controller::network_id() const noexcept
{
  execution_context context( _vm );
  return context.network_id();
}

state::head controller::head() const
{
  execution_context context( _vm );
  context.set_state_node( _db.head() );
  return context.head();
}

state::resource_limits controller::resource_limits() const
{
  execution_context context( _vm );
  context.set_state_node( _db.head() );
  return context.resource_limits();
}

std::uint64_t controller::account_resources( const protocol::account& account ) const
{
  execution_context context( _vm );
  context.set_state_node( _db.head() );
  return context.account_resources( account );
}

result< protocol::program_output > controller::read_program( const protocol::account& account,
                                                             const protocol::program_input& input ) const
{
  execution_context context( _vm );
  context.set_state_node( _db.head() );

  state::resource_limits limits;
  limits.compute_bandwidth_limit = _read_compute_bandwidth_limit;
  context.resource_meter().set_resource_limits( limits );

  auto output = context.run_program< tolerance::relaxed >( account, input.stdin, input.arguments );
  if( !output )
    return std::unexpected( output.error() );

  assert( output.value() );
  return *output.value();
}

std::uint64_t controller::account_nonce( const protocol::account& account ) const
{
  execution_context context( _vm );
  context.set_state_node( _db.head() );
  return context.account_nonce( account );
}

} // namespace respublica::controller
