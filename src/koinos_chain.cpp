#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <yaml-cpp/yaml.h>

#include <google/protobuf/util/json_util.h>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log/log.hpp>

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/options.hpp>
#include <koinos/util/random.hpp>
#include <koinos/util/services.hpp>

#include <git_version.hpp>

#define FIFO_ALGORITHM       "fifo"
#define BLOCK_TIME_ALGORITHM "block-time"
#define POB_ALGORITHM        "pob"

#define HELP_OPTION                               "help"
#define VERSION_OPTION                            "version"
#define BASEDIR_OPTION                            "basedir"
#define LOG_LEVEL_OPTION                          "log-level"
#define LOG_LEVEL_DEFAULT                         "info"
#define LOG_DIR_OPTION                            "log-dir"
#define LOG_DIR_DEFAULT                           ""
#define LOG_COLOR_OPTION                          "log-color"
#define LOG_COLOR_DEFAULT                         true
#define LOG_DATETIME_OPTION                       "log-datetime"
#define LOG_DATETIME_DEFAULT                      false
#define STATEDIR_OPTION                           "statedir"
#define JOBS_OPTION                               "jobs"
#define JOBS_DEFAULT                              uint64_t( 2 )
#define STATEDIR_DEFAULT                          "blockchain"
#define RESET_OPTION                              "reset"
#define GENESIS_DATA_FILE_OPTION                  "genesis-data"
#define GENESIS_DATA_FILE_DEFAULT                 "genesis_data.json"
#define READ_COMPUTE_BANDWITH_LIMIT_OPTION        "read-compute-bandwidth-limit"
#define READ_COMPUTE_BANDWITH_LIMIT_DEFAULT       10'000'000
#define SYSTEM_CALL_BUFFER_SIZE_OPTION            "system-call-buffer-size"
#define SYSTEM_CALL_BUFFER_SIZE_DEFAULT           64'000
#define FORK_ALGORITHM_OPTION                     "fork-algorithm"
#define FORK_ALGORITHM_DEFAULT                    FIFO_ALGORITHM
#define DISABLE_PENDING_TRANSACTION_LIMIT_OPTION  "disable-pending-transaction-limit"
#define DISABLE_PENDING_TRANSACTION_LIMIT_DEFAULT false
#define PENDING_TRANSACTION_LIMIT_OPTION          "pending-transaction-limit"
#define PENDING_TRANSACTION_LIMIT_DEFAULT         10
#define VERIFY_BLOCKS_OPTION                      "verify-blocks"
#define VERIFY_BLOCKS_DEFAULT                     false

using namespace boost;
using namespace koinos;

const std::string& version_string();

int main( int argc, char** argv )
{
  std::string log_level, log_dir, instance_id, fork_algorithm_option;
  std::filesystem::path statedir, genesis_data_file;
  uint64_t jobs, read_compute_limit, pending_transaction_limit;
  uint32_t syscall_bufsize;
  chain::genesis_data genesis_data;
  bool reset, log_color, log_datetime, disable_pending_transaction_limit, verify_blocks;
  chain::fork_resolution_algorithm fork_algorithm;

  try
  {
    program_options::options_description options;

    // clang-format off
    options.add_options()
      ( HELP_OPTION ",h"                        , "Print this help message and exit" )
      ( VERSION_OPTION ",v"                     , "Print version string and exit" )
      ( BASEDIR_OPTION ",d"                     , program_options::value< std::string >()->default_value( util::get_default_base_directory().string() ), "Koinos base directory" )
      ( LOG_LEVEL_OPTION ",l"                   , program_options::value< std::string >(), "The log filtering level" )
      ( JOBS_OPTION ",j"                        , program_options::value< uint64_t >()   , "The number of worker jobs" )
      ( READ_COMPUTE_BANDWITH_LIMIT_OPTION ",b" , program_options::value< uint64_t >()   , "The compute bandwidth when reading contracts via the API" )
      ( GENESIS_DATA_FILE_OPTION ",g"           , program_options::value< std::string >(), "The genesis data file" )
      ( STATEDIR_OPTION                         , program_options::value< std::string >(), "The location of the blockchain state files (absolute path or relative to basedir/chain)" )
      ( RESET_OPTION                            , program_options::value< bool >()       , "Reset the database" )
      ( FORK_ALGORITHM_OPTION ",f"              , program_options::value< std::string >(), "The fork resolution algorithm to use. Can be 'fifo', 'pob', or 'block-time'. (Default: 'fifo')" )
      ( LOG_DIR_OPTION                          , program_options::value< std::string >(), "The logging directory" )
      ( LOG_COLOR_OPTION                        , program_options::value< bool >()       , "Log color toggle" )
      ( LOG_DATETIME_OPTION                     , program_options::value< bool >()       , "Log datetime on console toggle" )
      ( SYSTEM_CALL_BUFFER_SIZE_OPTION          , program_options::value< uint32_t >()   , "System call RPC invocation buffer size" )
      ( DISABLE_PENDING_TRANSACTION_LIMIT_OPTION, program_options::value< bool >()       , "Disable the pending transaction limit")
      ( PENDING_TRANSACTION_LIMIT_OPTION        , program_options::value< uint64_t >()   , "Pending transaction limit per address (Default: 10)" )
      ( VERIFY_BLOCKS_OPTION                    , program_options::value< bool >()       , "Verify block receipts on reindex" );
    // clang-format on

    program_options::variables_map args;
    program_options::store( program_options::parse_command_line( argc, argv, options ), args );

    if( args.count( HELP_OPTION ) )
    {
      std::cout << options << std::endl;
      return EXIT_SUCCESS;
    }

    if( args.count( VERSION_OPTION ) )
    {
      const auto& v_str = version_string();
      std::cout.write( v_str.c_str(), v_str.size() );
      std::cout << std::endl;
      return EXIT_SUCCESS;
    }

    auto basedir = std::filesystem::path( args[ BASEDIR_OPTION ].as< std::string >() );
    if( basedir.is_relative() )
      basedir = std::filesystem::current_path() / basedir;

    YAML::Node config;
    YAML::Node global_config;
    YAML::Node chain_config;

    auto yaml_config = basedir / "config.yml";
    if( !std::filesystem::exists( yaml_config ) )
    {
      yaml_config = basedir / "config.yaml";
    }

    if( std::filesystem::exists( yaml_config ) )
    {
      config        = YAML::LoadFile( yaml_config );
      global_config = config[ "global" ];
      chain_config  = config[ util::service::chain ];
    }

    // clang-format off
    log_level                         = util::get_option< std::string >( LOG_LEVEL_OPTION, LOG_LEVEL_DEFAULT, args, chain_config, global_config );
    log_dir                           = util::get_option< std::string >( LOG_DIR_OPTION, LOG_DIR_DEFAULT, args, chain_config, global_config );
    log_color                         = util::get_option< bool >( LOG_COLOR_OPTION, LOG_COLOR_DEFAULT, args, chain_config, global_config );
    log_datetime                      = util::get_option< bool >( LOG_DATETIME_OPTION, LOG_DATETIME_DEFAULT, args, chain_config, global_config );
    statedir                          = std::filesystem::path( util::get_option< std::string >( STATEDIR_OPTION, STATEDIR_DEFAULT, args, chain_config, global_config ) );
    genesis_data_file                 = std::filesystem::path( util::get_option< std::string >( GENESIS_DATA_FILE_OPTION, GENESIS_DATA_FILE_DEFAULT, args, chain_config, global_config ) );
    reset                             = util::get_option< bool >( RESET_OPTION, false, args, chain_config, global_config );
    jobs                              = util::get_option< uint64_t >( JOBS_OPTION, std::max( JOBS_DEFAULT, uint64_t( std::thread::hardware_concurrency() ) ), args, chain_config, global_config );
    read_compute_limit                = util::get_option< uint64_t >( READ_COMPUTE_BANDWITH_LIMIT_OPTION, READ_COMPUTE_BANDWITH_LIMIT_DEFAULT, args, chain_config, global_config );
    fork_algorithm_option             = util::get_option< std::string >( FORK_ALGORITHM_OPTION, FORK_ALGORITHM_DEFAULT, args, chain_config, global_config );
    syscall_bufsize                   = util::get_option< uint32_t >( SYSTEM_CALL_BUFFER_SIZE_OPTION, SYSTEM_CALL_BUFFER_SIZE_DEFAULT, args, chain_config, global_config );
    disable_pending_transaction_limit = util::get_option< bool >( DISABLE_PENDING_TRANSACTION_LIMIT_OPTION, DISABLE_PENDING_TRANSACTION_LIMIT_DEFAULT, args, chain_config, global_config );
    pending_transaction_limit         = util::get_option< uint64_t >( PENDING_TRANSACTION_LIMIT_OPTION, PENDING_TRANSACTION_LIMIT_DEFAULT, args, chain_config, global_config );
    verify_blocks                     = util::get_option< bool >( VERIFY_BLOCKS_OPTION, VERIFY_BLOCKS_DEFAULT, args, chain_config, global_config );
    // clang-format on

    std::optional< std::filesystem::path > logdir_path;
    if( !log_dir.empty() )
    {
      logdir_path = std::make_optional< std::filesystem::path >( log_dir );
      if( logdir_path->is_relative() )
        logdir_path = basedir / util::service::chain / *logdir_path;
    }

    koinos::log::initialize( util::service::chain, log_level, logdir_path, log_color, log_datetime );

    LOG( info ) << version_string();

    if( jobs <= 1 )
      throw std::runtime_error( "jobs must be greater than 1" );

    if( config.IsNull() )
    {
      LOG( warning ) << "Could not find config (config.yml or config.yaml expected). Using default values";
    }

    if( fork_algorithm_option == FIFO_ALGORITHM )
    {
      LOG( info ) << "Using fork resolution algorithm: " << FIFO_ALGORITHM;
      fork_algorithm = chain::fork_resolution_algorithm::fifo;
    }
    else if( fork_algorithm_option == BLOCK_TIME_ALGORITHM )
    {
      LOG( info ) << "Using fork resolution algorithm: " << BLOCK_TIME_ALGORITHM;
      fork_algorithm = chain::fork_resolution_algorithm::block_time;
    }
    else if( fork_algorithm_option == POB_ALGORITHM )
    {
      LOG( info ) << "Using fork resolution algorithm: " << POB_ALGORITHM;
      fork_algorithm = chain::fork_resolution_algorithm::pob;
    }
    else
    {
      throw std::runtime_error( fork_algorithm_option + " is not a valid fork algorithm" );
    }

    if( statedir.is_relative() )
      statedir = basedir / util::service::chain / statedir;

    if( !std::filesystem::exists( statedir ) )
      std::filesystem::create_directories( statedir );

    // Load genesis data
    if( genesis_data_file.is_relative() )
      genesis_data_file = basedir / util::service::chain / genesis_data_file;

    if( !std::filesystem::exists( genesis_data_file ) )
      throw std::runtime_error( "unable to locate genesis data file at " + genesis_data_file.string() );

    std::ifstream gifs( genesis_data_file );
    std::stringstream genesis_data_stream;
    genesis_data_stream << gifs.rdbuf();
    std::string genesis_json = genesis_data_stream.str();
    gifs.close();

    google::protobuf::util::JsonParseOptions jpo;
    [[maybe_unused]]
    auto error_code = google::protobuf::util::JsonStringToMessage( genesis_json, &genesis_data, jpo );

    auto chain_id = crypto::hash( crypto::multicodec::sha2_256, genesis_data );
    if( chain_id.error() )
      throw std::logic_error( "chain ID is not valid" );

    LOG( info ) << "Chain ID: " << chain_id.value();
    LOG( info ) << "Number of jobs: " << jobs;
  }
  catch( const std::exception& e )
  {
    LOG( error ) << "Invalid argument: " << e.what();
    return EXIT_FAILURE;
  }

  std::atomic< bool > stopped = false;
  int retcode                 = EXIT_SUCCESS;
  std::vector< boost::thread > threads;

  asio::io_context main_ioc;
  chain::controller controller( read_compute_limit );

  try
  {
    asio::signal_set signals( main_ioc );
    signals.add( SIGINT );
    signals.add( SIGTERM );
#if defined( SIGQUIT )
    signals.add( SIGQUIT );
#endif

    signals.async_wait(
      [ & ]( const system::error_code& err, int num )
      {
        LOG( info ) << "Caught signal, shutting down...";
        stopped = true;
        main_ioc.stop();
      } );

    boost::thread::attributes attrs;
    attrs.set_stack_size( 8'192 * 1'024 );

    controller.open( statedir, genesis_data, fork_algorithm, reset );

    main_ioc.run();
  }
  catch( const std::exception& e )
  {
    if( !stopped )
    {
      LOG( fatal ) << "An unexpected error has occurred: " << e.what();
      retcode = EXIT_FAILURE;
    }
  }
  catch( const boost::exception& e )
  {
    LOG( fatal ) << "An unexpected error has occurred: " << boost::diagnostic_information( e );
    retcode = EXIT_FAILURE;
  }
  catch( ... )
  {
    LOG( fatal ) << "An unexpected error has occurred";
    retcode = EXIT_FAILURE;
  }

  controller.close();

  for( auto& t: threads )
    t.join();

  LOG( info ) << "Shut down gracefully";

  return retcode;
}

const std::string& version_string()
{
  static std::string v_str = "Koinos Chain v";
  v_str += std::to_string( PROJECT_MAJOR_VERSION ) + "." + std::to_string( PROJECT_MINOR_VERSION ) + "."
           + std::to_string( PROJECT_PATCH_VERSION );
  v_str += " (" + std::string( GIT_HASH ) + ")";
  return v_str;
}
