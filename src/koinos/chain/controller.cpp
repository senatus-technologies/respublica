#include <koinos/block_store/block_store.pb.h>
#include <koinos/broadcast/broadcast.pb.h>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>

#include <koinos/log/log.hpp>

#include <koinos/protocol/protocol.pb.h>

#include <koinos/rpc/chain/chain_rpc.pb.h>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/services.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <thread>

#include <boost/interprocess/streams/vectorstream.hpp>

namespace koinos::chain {

using namespace std::string_literals;
using namespace std::chrono_literals;

using fork_data = std::pair< std::vector< block_topology >, block_topology >;
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
                       const chain::genesis_data& data,
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
    p,
    [ & ]( state_db::state_node_ptr root )
    {
      // Write genesis objects into the database
      for( const auto& entry: data.entries() )
      {
        if( root->get_object( entry.space(), entry.key() ) )
          throw std::runtime_error( "encountered unexpected object in initial state" );

        root->put_object( entry.space(), entry.key(), &entry.value() );
      }
      LOG( info ) << "Wrote " << data.entries().size() << " genesis objects into new database";

      // Read genesis public key from the database, assert its existence at the correct location
      if( !root->get_object( state::space::metadata(), state::key::genesis_key ) )
        throw std::runtime_error( "could not find genesis public key in database" );

      // Calculate and write the chain ID into the database
      auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, data );
      if( !chain_id )
        throw std::runtime_error( "could not calculate chain id" );

      LOG( info ) << "Calculated chain ID: " << *chain_id;
      auto chain_id_str = util::converter::as< std::string >( *chain_id );
      if( root->get_object( chain::state::space::metadata(), chain::state::key::chain_id ) )
        throw std::runtime_error( "encountered unexpected chain id in initial state" );

      root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id_str );
      LOG( info ) << "Wrote chain ID into new database";
    },
    comp,
    _db.get_unique_lock() );

  if( reset )
  {
    LOG( info ) << "Resetting database...";
    _db.reset( _db.get_unique_lock() );
  }

  auto head = _db.get_head( _db.get_shared_lock() );
  LOG( info ) << "Opened database at block - Height: " << head->revision() << ", ID: " << head->id();
}

void controller::close()
{
  _db.close( _db.get_unique_lock() );
}

error controller::validate_block( const protocol::block& b )
{
  if( b.id().size() == 0 || !b.has_header() || b.header().previous().size() == 0 || b.header().height() == 0
      || b.header().timestamp() == 0 || b.header().previous_state_merkle_root().size() == 0
      || b.header().transaction_merkle_root().size() == 0 || b.signature().size() == 0 )
    return error( error_code::missing_required_arguments );

  for( const auto& t: b.transactions() )
    if( auto error = validate_transaction( t ); error )
      return error;

  return {};
}

error controller::validate_transaction( const protocol::transaction& t )
{
  if( t.id().size() == 0 || !t.has_header() || t.header().payer().size() == 0 || t.header().rc_limit() == 0
      || t.header().operation_merkle_root().size() == 0 || t.signatures_size() == 0 )
    return error( error_code::missing_required_arguments );

  return {};
}

std::expected< rpc::chain::submit_block_response, error >
controller::submit_block( const rpc::chain::submit_block_request& request,
                          uint64_t index_to,
                          std::chrono::system_clock::time_point now )
{
  const auto& block = request.block();

  if( auto error = validate_block( block ); error )
    return std::unexpected( error );

  static constexpr uint64_t index_message_interval = 1'000;
  static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );
  static constexpr std::chrono::seconds live_delta = std::chrono::seconds( 60 );

  auto time_lower_bound = uint64_t( 0 );
  auto time_upper_bound =
    std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();
  uint64_t parent_height = 0;

  auto db_lock = _db.get_shared_lock();

  auto block_id     = util::converter::to< crypto::multihash >( block.id() );
  auto block_height = block.header().height();
  auto parent_id    = util::converter::to< crypto::multihash >( block.header().previous() );
  auto block_node   = _db.get_node( block_id, db_lock );
  auto parent_node  = _db.get_node( parent_id, db_lock );

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
    block.header().timestamp()
    > std::chrono::duration_cast< std::chrono::milliseconds >( ( now - live_delta ).time_since_epoch() ).count();

  if( !index_to && live )
    LOG( debug ) << "Pushing block - Height: " << block_height << ", ID: " << block_id;

  block_node = _db.create_writable_node( parent_id, block_id, block.header(), db_lock );

  // If this is not the genesis case, we must ensure that the proposed block timestamp is greater
  // than the parent block timestamp.
  if( block_node && !parent_id.is_zero() )
  {
    execution_context parent_ctx( _vm_backend );

    parent_ctx.set_state_node( parent_node );
    auto head_info   = parent_ctx.get_head_info();
    parent_height    = head_info.head_topology().height();
    time_lower_bound = head_info.head_block_time();
  }

  execution_context ctx( _vm_backend, intent::block_application );

  if( ( parent_id.is_zero() && block_height != 1 ) || ( block_height != parent_height + 1 ) )
    return std::unexpected( error_code::unexpected_height );

  if( !block_node )
    return std::unexpected( error_code::block_state_error );

  if( ( block.header().timestamp() > time_upper_bound ) || ( block.header().timestamp() <= time_lower_bound ) )
    return std::unexpected( error_code::timestamp_out_of_bounds );

  if( block.header().previous_state_merkle_root() != util::converter::as< std::string >( parent_node->merkle_root() ) )
    return std::unexpected( error_code::state_merkle_mismatch );

  ctx.set_state_node( block_node );

  return ctx.apply_block( block ).and_then(
    [ & ]( auto&& receipt ) -> std::expected< rpc::chain::submit_block_response, error >
    {
      rpc::chain::submit_block_response resp;
      *resp.mutable_receipt() = std::move( receipt );

      if( !index_to && live )
      {
        auto num_transactions = block.transactions_size();

        LOG( info ) << "Block applied - Height: " << block_height << ", ID: " << block_id << " (" << num_transactions
                    << ( num_transactions == 1 ? " transaction)" : " transactions)" );
      }
      else if( block_height % index_message_interval == 0 )
      {
        if( index_to )
        {
          auto progress = block_height / static_cast< double >( index_to ) * 100;
          LOG( info ) << "Indexing chain (" << progress << "%) - Height: " << block_height << ", ID: " << block_id;
        }
        else
        {
          auto to_go = std::chrono::duration_cast< std::chrono::seconds >(
                         now.time_since_epoch() - std::chrono::milliseconds( block.header().timestamp() ) )
                         .count();
          LOG( info ) << "Sync progress - Height: " << block_height << ", ID: " << block_id << " ("
                      << format_time( to_go ) << " block time remaining)";
        }
      }

      auto lib = ctx.get_last_irreversible_block();

      // We need to finalize our node, checking if it is the new head block, update the cached head block,
      // and advancing LIB as an atomic action or else we risk _db.get_head(), _cached_head_block, and
      // LIB desyncing from each other
      db_lock.reset();
      block_node.reset();
      parent_node.reset();
      ctx.clear_state_node();

      auto unique_db_lock = _db.get_unique_lock();
      _db.finalize_node( block_id, unique_db_lock );

      resp.mutable_receipt()->set_state_merkle_root(
        util::converter::as< std::string >( _db.get_node( block_id, unique_db_lock )->merkle_root() ) );

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

      return resp;
    } );
}

void controller::apply_block_delta( const protocol::block& block,
                                    const protocol::block_receipt& receipt,
                                    uint64_t index_to )
{
  uint64_t index_message_interval                  = std::max( 10'000ull, index_to / 1'000ull );
  static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );

  auto db_lock = _db.get_shared_lock();

  auto block_id     = util::converter::to< crypto::multihash >( block.id() );
  auto block_height = block.header().height();
  auto parent_id    = util::converter::to< crypto::multihash >( block.header().previous() );
  auto block_node   = _db.get_node( block_id, db_lock );
  auto parent_node  = _db.get_node( parent_id, db_lock );

  if( block_node )
  {
    block_node.reset();
    _db.discard_node( block_id, db_lock );
  }

  // This prevents returning "unknown previous block" when the pushed block is the LIB
  if( !parent_node )
  {
    auto root = _db.get_root( db_lock );
    if( block_height < root->revision() )
      throw std::runtime_error( "block is before irreversiblity" );

    if( block_id != root->id() )
      throw std::runtime_error( "unknown previous block" );

    return; // Block is current LIB
  }

  block_node = _db.create_writable_node( parent_id, block_id, block.header(), db_lock );

  execution_context ctx( _vm_backend, intent::block_application );
  ctx.set_state_node( block_node );

  for( const auto& delta_entry: receipt.state_delta_entries() )
  {
    chain::object_space object_space;
    object_space.set_system( delta_entry.object_space().system() );
    object_space.set_zone( delta_entry.object_space().zone() );
    object_space.set_id( delta_entry.object_space().id() );

    if( delta_entry.has_value() )
      block_node->put_object( object_space, delta_entry.key(), &delta_entry.value() );
    else
      block_node->remove_object( object_space, delta_entry.key() );
  }

  if( block_height % index_message_interval == 0 )
  {
    auto progress = block_height / static_cast< double >( index_to ) * 100;
    LOG( info ) << "Indexing chain (" << progress << "%) - Height: " << block_height << ", ID: " << block_id;
  }

  auto lib = ctx.get_last_irreversible_block();

  // We need to finalize our node, checking if it is the new head block, update the cached head block,
  // and advancing LIB as an atomic action or else we risk _db.get_head(), _cached_head_block, and
  // LIB desyncing from each other
  db_lock.reset();
  block_node.reset();
  parent_node.reset();
  ctx.clear_state_node();

  auto unique_db_lock = _db.get_unique_lock();
  _db.finalize_node( block_id, unique_db_lock );

  if( block_id == _db.get_head( unique_db_lock )->id() )
  {
    std::unique_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
    _cached_head_block = std::make_shared< protocol::block >( block );
  }

  if( lib > _db.get_root( unique_db_lock )->revision() )
  {
    auto lib_id = _db.get_node_at_revision( lib, block_id, unique_db_lock )->id();
    _db.commit_node( lib_id, unique_db_lock );
  }
}

std::expected< rpc::chain::submit_transaction_response, error >
controller::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
  if( auto error = validate_transaction( request.transaction() ); error )
    return std::unexpected( error );
  auto transaction_id = util::to_hex( request.transaction().id() );

  LOG( debug ) << "Pushing transaction - ID: " << transaction_id;

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
  ctx.resource_meter().set_resource_limit_data( ctx.get_resource_limits() );

  return ctx.apply_transaction( request.transaction() )
    .and_then(
      [ & ]( auto&& receipt ) -> std::expected< rpc::chain::submit_transaction_response, error >
      {
        LOG( debug ) << "Transaction applied - ID: " << transaction_id;

        rpc::chain::submit_transaction_response resp;
        *resp.mutable_receipt() = std::move( receipt );
        return resp;
      } );
}

std::expected< rpc::chain::get_chain_id_response, error >
controller::get_chain_id( const rpc::chain::get_chain_id_request& )
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );

  auto chain_id = ctx.get_chain_id();

  rpc::chain::get_chain_id_response resp;
  resp.set_chain_id( std::string( reinterpret_cast< const char* >( chain_id.data() ), chain_id.size() ) );
  return resp;
}

std::expected< rpc::chain::get_head_info_response, error >
controller::get_head_info( const rpc::chain::get_head_info_request& )
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );

  auto head_info = ctx.get_head_info();

  rpc::chain::get_head_info_response resp;
  *resp.mutable_head_topology() = head_info.head_topology();
  resp.set_head_block_time( head_info.head_block_time() );
  resp.set_last_irreversible_block( resp.last_irreversible_block() );
  resp.set_head_state_merkle_root( util::converter::as< std::string >( ctx.get_state_merkle_root() ) );
  return resp;
}

std::expected< rpc::chain::get_resource_limits_response, error >
controller::get_resource_limits( const rpc::chain::get_resource_limits_request& )
{
  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );

  rpc::chain::get_resource_limits_response resp;
  *resp.mutable_resource_limit_data() = ctx.get_resource_limits();
  return resp;
}

std::expected< rpc::chain::get_account_rc_response, error >
controller::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
  if( request.account().size() == 0 )
    return std::unexpected( error_code::missing_required_arguments );

  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );

  rpc::chain::get_account_rc_response resp;
  resp.set_rc( ctx.get_account_rc(
    bytes_s( reinterpret_cast< const std::byte* >( request.account().data() ),
             reinterpret_cast< const std::byte* >( request.account().data() ) + request.account().size() ) ) );
  return resp;
}

std::expected< rpc::chain::read_contract_response, error >
controller::read_contract( const rpc::chain::read_contract_request& request )
{
  if( request.contract_id().size() == 0 )
    return std::unexpected( error_code::missing_required_arguments );

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

  resource_limit_data rl;
  rl.set_compute_bandwidth_limit( _read_compute_bandwidth_limit );
  ctx.resource_meter().set_resource_limit_data( rl );

  rpc::chain::read_contract_response resp;

  std::vector< bytes_s > args;
  args.reserve( request.args_size() );

  for( const auto& arg: request.args() )
    args.emplace_back( bytes_s( reinterpret_cast< const std::byte* >( arg.data() ),
                                reinterpret_cast< const std::byte* >( arg.data() ) + arg.size() ) );

  return ctx
    .call_program(
      bytes_s( reinterpret_cast< const std::byte* >( request.contract_id().data() ),
               reinterpret_cast< const std::byte* >( request.contract_id().data() ) + request.contract_id().size() ),
      request.entry_point(),
      args )
    .and_then(
      [ &ctx ]( auto&& result ) -> std::expected< rpc::chain::read_contract_response, error >
      {
        rpc::chain::read_contract_response resp;

        resp.set_result( std::string( reinterpret_cast< const char* >( result.data() ), result.size() ) );

        for( const auto& message: ctx.chronicler().logs() )
          *resp.add_logs() = message;

        return resp;
      } );
}

std::expected< rpc::chain::get_account_nonce_response, error >
controller::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
  if( request.account().size() == 0 )
    return std::unexpected( error_code::missing_required_arguments );

  execution_context ctx( _vm_backend );
  ctx.set_state_node( _db.get_head( _db.get_shared_lock() ) );

  rpc::chain::get_account_nonce_response resp;
  resp.set_nonce( ctx.get_account_nonce(
    bytes_s( reinterpret_cast< const std::byte* >( request.account().data() ),
             reinterpret_cast< const std::byte* >( request.account().data() ) + request.account().size() ) ) );
  return resp;
}

} // namespace koinos::chain
