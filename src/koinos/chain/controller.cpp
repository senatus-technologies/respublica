#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>

#include <koinos/log/log.hpp>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/services.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <span>

#include <boost/interprocess/streams/vectorstream.hpp>

namespace koinos::chain {

using namespace std::string_literals;
using namespace std::chrono_literals;

using koinos::error::error_code;

std::string format_time( int64_t time )
{
  std::stringstream ss;

  auto seconds  = time % 60;
  time         /= 60;
  auto minutes  = time % 60;
  time         /= 60;
  auto hours    = time % 24;
  time         /= 24;
  auto days     = time % 365;
  auto years    = time / 365;

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
  uint64_t index_to;
  std::chrono::system_clock::time_point application_time;
  bool propose_block;
};

struct apply_block_result
{
  std::optional< protocol::block_receipt > receipt;
  std::vector< uint32_t > failed_transaction_indices;
};

controller::controller( uint64_t read_compute_bandwidth_limit ):
    _read_compute_bandwidth_limit( read_compute_bandwidth_limit )
{
  _vm_backend = vm_manager::get_vm_backend(); // Default is fizzy
  if( !_vm_backend )
    throw std::runtime_error( "could not get vm backend" );

  _cached_head_block = std::make_shared< const protocol::block >( protocol::block() );

  _vm_backend->initialize();
  LOG( info ) << "Initialized " << _vm_backend->backend_name() << " VM backend";
}

controller::~controller()
{
  close();
}

void controller::open( const std::filesystem::path& p,
                       const state::genesis_data& data,
                       fork_resolution_algorithm algo,
                       bool reset )
{
  state_db::state_node_comparator_function comp;

  switch( algo )
  {
    case fork_resolution_algorithm::block_time:
      comp = &state_db::block_time_comparator;
      break;
    case fork_resolution_algorithm::pob:
      comp = &state_db::pob_comparator;
      break;
    case fork_resolution_algorithm::fifo:
      [[fallthrough]];
    default:
      comp = &state_db::fifo_comparator;
  }

  _db.open(
    {},
    [ & ]( state_db::state_node_ptr root )
    {
      // Write genesis objects into the database
      for( const auto& entry: data )
      {
        if( root->get_object( entry.space, entry.key ) )
          throw std::runtime_error( "encountered unexpected object in initial state" );

        root->put_object( entry.space, entry.key, &entry.value );
      }
      LOG( info ) << "Wrote " << data.size() << " genesis objects into new database";

      // Read genesis public key from the database, assert its existence at the correct location
      if( !root->get_object( state::space::metadata(), state::key::genesis_key ) )
        throw std::runtime_error( "could not find genesis public key in database" );
    },
    comp,
    _db.get_unique_lock() );

  if( reset )
  {
    LOG( info ) << "Resetting database...";
    _db.reset( _db.get_unique_lock() );
  }

  auto head = _db.get_head( _db.get_shared_lock() );
  LOG( info ) << "Opened database at block - Height: " << head->revision() << ", ID: " << util::to_hex( head->id() );
}

void controller::close()
{
  _db.close( _db.get_unique_lock() );
}

std::expected< protocol::block_receipt, error >
controller::process( const protocol::block& block, uint64_t index_to, std::chrono::system_clock::time_point now )
{
  if( !block.validate() )
    return std::unexpected( error_code::malformed_block );

  static constexpr uint64_t index_message_interval = 1'000;
  static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );
  static constexpr std::chrono::seconds live_delta = std::chrono::seconds( 60 );
  static constexpr state_db::state_node_id zero_id{};

  auto time_lower_bound = uint64_t( 0 );
  auto time_upper_bound =
    std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();

  auto db_lock = _db.get_shared_lock();

  const auto& block_id  = block.id;
  auto block_height     = block.height;
  const auto& parent_id = block.previous;
  auto block_node       = _db.get_node( block_id, db_lock );
  auto parent_node      = _db.get_node( parent_id, db_lock );

  bool new_head = false;

  if( block_node )
    return {}; // Block has been applied

  // This prevents returning "unknown previous block" when the pushed block is the LIB
  if( !parent_node )
  {
    auto root = _db.get_root( db_lock );
    if( block_height < root->revision() )
      return std::unexpected( error_code::pre_irreversibility_block );

    if( block_id != root->id() )
      return std::unexpected( error_code::unknown_previous_block );

    return {}; // Block is current LIB
  }
  else if( !parent_node->is_finalized() )
    return std::unexpected( error_code::unknown_previous_block );

  bool live =
    block.timestamp
    > std::chrono::duration_cast< std::chrono::milliseconds >( ( now - live_delta ).time_since_epoch() ).count();

  if( !index_to && live )
    LOG( debug ) << "Pushing block - Height: " << block_height << ", ID: " << util::to_hex( block_id );

  block_node = _db.create_writable_node( parent_id, block_id, block.timestamp, db_lock );

  if( !block_node )
    return std::unexpected( error_code::block_state_error );

  if( parent_id == zero_id )
  {
    if( block_height != 1 )
      return std::unexpected( error_code::unexpected_height );
  }
  else
  {
    if( block.state_merkle_root != parent_node->merkle_root() )
      return std::unexpected( error_code::state_merkle_mismatch );

    execution_context parent_ctx( _vm_backend );

    parent_ctx.set_state_node( parent_node );
    auto parent_info = parent_ctx.head();
    time_lower_bound = parent_info.time;

    if( block_height != parent_info.height + 1 )
      return std::unexpected( error_code::unexpected_height );
  }

  if( ( block.timestamp > time_upper_bound ) || ( block.timestamp <= time_lower_bound ) )
    return std::unexpected( error_code::timestamp_out_of_bounds );

  execution_context ctx( _vm_backend, intent::block_application );
  ctx.set_state_node( block_node );

  return ctx.apply( block ).and_then(
    [ & ]( auto&& receipt ) -> std::expected< protocol::block_receipt, error >
    {
      if( !index_to && live )
      {
        auto num_transactions = block.transactions.size();

        LOG( info ) << "Block applied - Height: " << block_height << ", ID: " << util::to_hex( block_id ) << " ("
                    << num_transactions << ( num_transactions == 1 ? " transaction)" : " transactions)" );
      }
      else if( block_height % index_message_interval == 0 )
      {
        if( index_to )
        {
          auto progress = block_height / static_cast< double >( index_to ) * 100;
          LOG( info ) << "Indexing chain (" << progress << "%) - Height: " << block_height
                      << ", ID: " << util::to_hex( block_id );
        }
        else
        {
          auto to_go = std::chrono::duration_cast< std::chrono::seconds >(
                         now.time_since_epoch() - std::chrono::milliseconds( block.timestamp ) )
                         .count();
          LOG( info ) << "Sync progress - Height: " << block_height << ", ID: " << util::to_hex( block_id ) << " ("
                      << format_time( to_go ) << " block time remaining)";
        }
      }

      auto lib = ctx.last_irreversible_block();

      // We need to finalize our node, checking if it is the new head block, update the cached head block,
      // and advancing LIB as an atomic action or else we risk _db.get_head(), _cached_head_block, and
      // LIB desyncing from each other
      db_lock.reset();
      block_node.reset();
      parent_node.reset();
      ctx.clear_state_node();

      auto unique_db_lock = _db.get_unique_lock();
      _db.finalize_node( block_id, unique_db_lock );

      auto merkle_root = _db.get_node( block_id, unique_db_lock )->merkle_root();
      assert( merkle_root.size() == receipt.state_merkle_root.size() );
      std::copy( merkle_root.begin(), merkle_root.end(), receipt.state_merkle_root.begin() );

      if( block_id == _db.get_head( unique_db_lock )->id() )
      {
        std::unique_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
        new_head           = true;
        _cached_head_block = std::make_shared< protocol::block >( block );
      }

      if( lib > _db.get_root( unique_db_lock )->revision() )
      {
        auto lib_id = _db.get_node_at_revision( lib, block_id, unique_db_lock )->id();
        _db.commit_node( lib_id, unique_db_lock );
      }

      return receipt;
    } );
}

std::expected< protocol::transaction_receipt, error > controller::process( const protocol::transaction& transaction,
                                                                           bool broadcast )
{
  if( !transaction.validate() )
    return std::unexpected( error_code::malformed_transaction );
#if 0
  auto transaction_id = util::to_hex( transaction.id );
#endif

#pragma message( "Removed logging in transaction hot path, use quill?" )
#if 0
  LOG( debug ) << "Pushing transaction - ID: " << transaction_id;
#endif

  if( network_id() != transaction.network_id )
    return std::unexpected( error_code::failure );

  auto db_lock = _db.get_shared_lock();
  state_db::state_node_ptr head;
  execution_context ctx( _vm_backend, intent::transaction_application );
  std::shared_ptr< const protocol::block > head_block_ptr;

  {
    std::shared_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
    head_block_ptr = _cached_head_block;
    if( !head_block_ptr )
      throw std::runtime_error( "error retrieving head block" );

    head = _db.get_head( db_lock );
  }

  ctx.set_state_node( head->create_anonymous_node() );
  ctx.resource_meter().set_resource_limits( ctx.resource_limits() );

  return ctx.apply( transaction )
    .and_then(
      [ & ]( auto&& receipt ) -> std::expected< protocol::transaction_receipt, error >
      {
#pragma message( "Removed logging in transaction hot path, use quill?" )
#if 0
        LOG( debug ) << "Transaction applied - ID: " << transaction_id;
#endif
        return receipt;
      } );
}

crypto::digest controller::network_id() const
{
  execution_context ctx( _vm_backend );
  return ctx.network_id();
}

state::head controller::head() const
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );
  return ctx.head();
}

state::resource_limits controller::resource_limits() const
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );
  return ctx.resource_limits();
}

uint64_t controller::account_resources( const protocol::account& account ) const
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );
  return ctx.account_rc( account );
}

std::expected< protocol::program_output, error >
controller::read_program( const protocol::account& account,
                          uint64_t entry_point,
                          const std::vector< std::vector< std::byte > >& arguments ) const
{
  auto db_lock = _db.get_shared_lock();

  execution_context ctx( _vm_backend );
  std::shared_ptr< const protocol::block > head_block_ptr;

  {
    std::shared_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
    head_block_ptr = _cached_head_block;
    if( !head_block_ptr )
      throw std::runtime_error( "error retrieving head block" );

    ctx.set_state_node( _db.get_head( db_lock ) );
  }

  state::resource_limits rl;
  rl.compute_bandwidth_limit = _read_compute_bandwidth_limit;
  ctx.resource_meter().set_resource_limits( rl );

  std::vector< std::span< const std::byte > > args;
  args.reserve( arguments.size() );

  for( const auto& arg: arguments )
    args.emplace_back( std::span( arg ) );

  return ctx.call_program( account, entry_point, args )
    .and_then(
      [ &ctx ]( auto&& result ) -> std::expected< protocol::program_output, error >
      {
        protocol::program_output output;
        output.result = std::move( result );

        output.logs.reserve( ctx.chronicler().logs().size() );
        for( const auto& message: ctx.chronicler().logs() )
          output.logs.push_back( message );

        return output;
      } );
}

uint64_t controller::account_nonce( const protocol::account& account ) const
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );
  return ctx.account_nonce( account );
}

} // namespace koinos::chain
