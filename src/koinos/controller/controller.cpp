#include <koinos/controller/controller.hpp>
#include <koinos/controller/execution_context.hpp>
#include <koinos/controller/host_api.hpp>
#include <koinos/controller/state.hpp>

#include <koinos/encode.hpp>
#include <koinos/log.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <span>

#include <boost/interprocess/streams/vectorstream.hpp>

namespace koinos::controller {

using namespace std::string_literals;
using namespace std::chrono_literals;

constexpr auto one_hundred_percent = 100;

std::string format_time( std::int64_t time )
{
  std::stringstream ss;
  constexpr auto seconds_per_minute = 60;
  constexpr auto minutes_per_hour   = 60;
  constexpr auto hours_per_day      = 24;
  constexpr auto days_per_year      = 365;

  auto seconds  = time % seconds_per_minute;
  time         /= seconds_per_minute;
  auto minutes  = time % minutes_per_hour;
  time         /= minutes_per_hour;
  auto hours    = time % hours_per_day;
  time         /= hours_per_day;
  auto days     = time % days_per_year;
  auto years    = time / days_per_year;

  if( years )
  {
    ss << years << "y, " << days << "d, ";
  }
  else if( days )
  {
    ss << days << "d, ";
  }

  ss << std::setw( 2 ) << std::setfill( '0' ) << hours;
  ss << std::setw( 1 ) << "h, ";
  ss << std::setw( 2 ) << std::setfill( '0' ) << minutes;
  ss << std::setw( 1 ) << "m, ";
  ss << std::setw( 2 ) << std::setfill( '0' ) << seconds;
  ss << std::setw( 1 ) << "s";
  return ss.str();
}

struct apply_block_options
{
  std::uint64_t index_to = 0;
  std::chrono::system_clock::time_point application_time;
  bool propose_block = false;
};

struct apply_block_result
{
  std::optional< protocol::block_receipt > receipt;
  std::vector< std::uint32_t > failed_transaction_indices;
};

controller::controller( std::uint64_t read_compute_bandwidth_limit ):
    _read_compute_bandwidth_limit( read_compute_bandwidth_limit )
{
  _vm_backend = vm_manager::get_vm_backend(); // Default is fizzy
  if( !_vm_backend )
    throw std::runtime_error( "could not get vm backend" );

  _vm_backend->initialize();
  LOG( info ) << "Initialized " << _vm_backend->backend_name() << " VM backend";
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
      // Write genesis objects into the database
      for( const auto& entry: data )
      {
        if( root->get( entry.space, entry.key ) )
          throw std::runtime_error( "encountered unexpected object in initial state" );

        root->put( entry.space, entry.key, entry.value );
      }
      LOG( info ) << "Wrote " << data.size() << " genesis objects into new database";

      // Read genesis public key from the database, assert its existence at the correct location
      if( !root->get( state::space::metadata(), state::key::genesis_key() ) )
        throw std::runtime_error( "could not find genesis public key in database" );
    },
    algo );

  if( reset )
  {
    LOG( info ) << "Resetting database...";
    _db.reset();
  }

  auto head = _db.head();
  LOG( info ) << "Opened database at block - Height: " << head->revision() << ", ID: " << encode::to_hex( head->id() );
}

void controller::close()
{
  _db.close();
}

result< protocol::block_receipt >
controller::process( const protocol::block& block, std::uint64_t index_to, std::chrono::system_clock::time_point now )
{
  if( !block.validate() )
    return std::unexpected( controller_errc::malformed_block );

  static constexpr std::uint64_t index_message_interval = 1'000;
  static constexpr std::chrono::seconds time_delta      = std::chrono::seconds( 5 );
  static constexpr std::chrono::seconds live_delta      = std::chrono::seconds( 60 );
  static constexpr state_db::state_node_id zero_id{};

  auto time_lower_bound = std::uint64_t( 0 );
  auto time_upper_bound =
    std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();

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

  bool live =
    block.timestamp
    > std::chrono::duration_cast< std::chrono::milliseconds >( ( now - live_delta ).time_since_epoch() ).count();

  if( !index_to && live )
    LOG( debug ) << "Pushing block - Height: " << block_height << ", ID: " << encode::to_hex( block_id );

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

    execution_context parent_context( _vm_backend );

    parent_context.set_state_node( parent_node );
    auto parent_info = parent_context.head();
    time_lower_bound = parent_info.time;

    if( block_height != parent_info.height + 1 )
      return std::unexpected( controller_errc::unexpected_height );
  }

  if( ( block.timestamp > time_upper_bound ) || ( block.timestamp <= time_lower_bound ) )
    return std::unexpected( controller_errc::timestamp_out_of_bounds );

  execution_context context( _vm_backend, intent::block_application );
  context.set_state_node( block_node );

  return context.apply( block ).and_then(
    [ & ]( auto&& receipt ) -> result< protocol::block_receipt >
    {
      if( !index_to && live )
      {
        auto num_transactions = block.transactions.size();

        LOG( info ) << "Block applied - Height: " << block_height << ", ID: " << encode::to_hex( block_id ) << " ("
                    << num_transactions << ( num_transactions == 1 ? " transaction)" : " transactions)" );
      }
      else if( block_height % index_message_interval == 0 )
      {
        if( index_to )
        {
          auto progress =
            static_cast< double >( block_height ) / static_cast< double >( index_to ) * one_hundred_percent;
          LOG( info ) << "Indexing network (" << progress << "%) - Height: " << block_height
                      << ", ID: " << encode::to_hex( block_id );
        }
        else
        {
          auto to_go = std::chrono::duration_cast< std::chrono::seconds >(
                         now.time_since_epoch() - std::chrono::milliseconds( block.timestamp ) )
                         .count();
          LOG( info ) << "Sync progress - Height: " << block_height << ", ID: " << encode::to_hex( block_id ) << " ("
                      << format_time( to_go ) << " block time remaining)";
        }
      }

      constexpr auto default_irreversible_threshold = 60;
      auto irreversible_block                       = block_node->revision() > default_irreversible_threshold
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
#if 0
  auto transaction_id = encode::to_hex( transaction.id );
#endif

#pragma message( "Removed logging in transaction hot path, use quill?" )
#if 0
  LOG( debug ) << "Pushing transaction - ID: " << transaction_id;
#endif

  if( network_id() != transaction.network_id )
    return std::unexpected( controller_errc::network_id_mismatch );

  state_db::state_node_ptr head = _db.head();

  execution_context context( _vm_backend, intent::transaction_application );
  context.set_state_node( head->make_child() );
  context.resource_meter().set_resource_limits( context.resource_limits() );

  return context.apply( transaction )
    .and_then(
      [ & ]( auto&& receipt ) -> result< protocol::transaction_receipt >
      {
#pragma message( "Removed logging in transaction hot path, use quill?" )
#if 0
        LOG( debug ) << "Transaction applied - ID: " << transaction_id;
#endif
        return receipt;
      } );
}

const crypto::digest& controller::network_id() const noexcept
{
  execution_context context( _vm_backend );
  return context.network_id();
}

state::head controller::head() const
{
  execution_context context( _vm_backend );
  context.set_state_node( _db.head() );
  return context.head();
}

state::resource_limits controller::resource_limits() const
{
  execution_context context( _vm_backend );
  context.set_state_node( _db.head() );
  return context.resource_limits();
}

std::uint64_t controller::account_resources( const protocol::account& account ) const
{
  execution_context context( _vm_backend );
  context.set_state_node( _db.head() );
  return context.account_resources( account );
}

result< protocol::program_output >
controller::read_program( const protocol::account& account,
                          const std::vector< std::vector< std::byte > >& arguments ) const
{
  execution_context context( _vm_backend );
  context.set_state_node( _db.head() );

  state::resource_limits limits;
  limits.compute_bandwidth_limit = _read_compute_bandwidth_limit;
  context.resource_meter().set_resource_limits( limits );

  std::vector< std::span< const std::byte > > args;
  args.reserve( arguments.size() );

  for( const auto& arg: arguments )
    args.emplace_back( std::span( arg ) );

  return context.call_program( account, args )
    .and_then(
      [ &context ]( auto&& result ) -> koinos::controller::result< protocol::program_output >
      {
        protocol::program_output output;
        output.result = std::move( result );

        output.logs.reserve( context.chronicler().logs().size() );
        for( const auto& message: context.chronicler().logs() )
          output.logs.push_back( message );

        return output;
      } );
}

std::uint64_t controller::account_nonce( const protocol::account& account ) const
{
  execution_context context( _vm_backend );
  context.set_state_node( _db.head() );
  return context.account_nonce( account );
}

} // namespace koinos::controller
