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

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/crypto.hpp>
#include <koinos/log/log.hpp>

#include <koinos/util/base58.hpp>
#include <koinos/util/options.hpp>
#include <koinos/util/random.hpp>
#include <koinos/util/services.hpp>

#include <git_version.hpp>

namespace constants {

using namespace std::string_literals;

constexpr auto fifo_algorithm = "fifo"s;

constexpr auto help_option                          = "help,h"s;
constexpr auto version_option                       = "version,v"s;
constexpr auto basedir_option                       = "basedir,d"s;
constexpr auto log_level_option                     = "log-level,l"s;
constexpr auto log_level_default                    = "info"s;
constexpr auto log_dir_option                       = "log-dir"s;
constexpr auto log_dir_default                      = ""s;
constexpr auto log_color_option                     = "log-color"s;
constexpr auto log_color_default                    = true;
constexpr auto log_datetime_option                  = "log-datetime"s;
constexpr auto log_datetime_default                 = false;
constexpr auto statedir_option                      = "statedir"s;
constexpr auto statedir_default                     = "blockchain"s;
constexpr auto jobs_option                          = "jobs,j"s;
constexpr auto jobs_default                         = 2ul;
constexpr auto reset_option                         = "reset"s;
constexpr auto reset_default                        = false;
constexpr auto genesis_data_file_option             = "genesis-data,g"s;
const auto genesis_data_file_default                = "genesis_data.json"s;
const auto read_compute_bandwidth_limit_option      = "read-compute-bandwidth-limit,b"s;
constexpr auto read_compute_bandwidth_limit_default = 10'000'000;
const auto fork_algorithm_option                    = "fork-algorithm,f"s;
constexpr auto fork_algorithm_default               = fifo_algorithm;

constexpr auto eight_megabytes = 8 * 1'024 * 1'024;

} // namespace constants

using namespace boost;
using namespace koinos;

const std::string& version_string();

int main( int argc, char** argv )
{
  std::string log_level, log_dir, instance_id, fork_algorithm_option;
  std::filesystem::path statedir, genesis_data_file;
  uint64_t jobs = 0, read_compute_limit = 0;
  chain::state::genesis_data genesis_data;
  bool reset = false, log_color = false, log_datetime = false;
  state_db::fork_resolution_algorithm fork_algorithm = state_db::fork_resolution_algorithm::fifo;

  try
  {
    program_options::options_description options;

    // clang-format off
    options.add_options()
      ( constants::help_option.data()                        , "Print this help message and exit" )
      ( constants::version_option.data()                     , "Print version string and exit" )
      ( constants::basedir_option.data()                     , program_options::value< std::string >()->default_value( util::get_default_base_directory().string() ), "Koinos base directory" )
      ( constants::log_level_option.data()                   , program_options::value< std::string >(), "The log filtering level" )
      ( constants::jobs_option.data()                        , program_options::value< uint64_t >()   , "The number of worker jobs" )
      ( constants::read_compute_bandwidth_limit_option.data(), program_options::value< uint64_t >()   , "The compute bandwidth when reading contracts via the API" )
      ( constants::genesis_data_file_option.data()           , program_options::value< std::string >(), "The genesis data file" )
      ( constants::statedir_option.data()                    , program_options::value< std::string >(), "The location of the blockchain state files (absolute path or relative to basedir/chain)" )
      ( constants::reset_option.data()                       , program_options::value< bool >()       , "Reset the database" )
      ( constants::fork_algorithm_option.data()              , program_options::value< std::string >(), "The fork resolution algorithm to use. Can be 'fifo', 'pob', or 'block-time'. (Default: 'fifo')" )
      ( constants::log_dir_option.data()                     , program_options::value< std::string >(), "The logging directory" )
      ( constants::log_color_option.data()                   , program_options::value< bool >()       , "Log color toggle" )
      ( constants::log_datetime_option.data()                , program_options::value< bool >()       , "Log datetime on console toggle" );
    // clang-format on

    program_options::variables_map args;
    program_options::store( program_options::parse_command_line( argc, argv, options ), args );

    if( args.count( constants::help_option ) )
    {
      std::cout << options << '\n';
      return EXIT_SUCCESS;
    }

    if( args.count( constants::version_option ) )
    {
      std::cout << version_string() << "\n";
      return EXIT_SUCCESS;
    }

    auto basedir = std::filesystem::path( args[ constants::basedir_option ].as< std::string >() );
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
    log_level                         = util::get_option< std::string >( constants::log_level_option, constants::log_level_default, args, chain_config, global_config );
    log_dir                           = util::get_option< std::string >( constants::log_dir_option, constants::log_dir_default, args, chain_config, global_config );
    log_color                         = util::get_option< bool >( constants::log_color_option, constants::log_color_default, args, chain_config, global_config );
    log_datetime                      = util::get_option< bool >( constants::log_datetime_option, constants::log_datetime_default, args, chain_config, global_config );
    statedir                          = std::filesystem::path( util::get_option< std::string >( constants::statedir_option, constants::statedir_default, args, chain_config, global_config ) );
    genesis_data_file                 = std::filesystem::path( util::get_option< std::string >( constants::genesis_data_file_option, constants::genesis_data_file_default, args, chain_config, global_config ) );
    reset                             = util::get_option< bool >( constants::reset_option, constants::reset_default, args, chain_config, global_config );
    jobs                              = util::get_option< uint64_t >( constants::jobs_option, std::max( constants::jobs_default, uint64_t( std::thread::hardware_concurrency() ) ), args, chain_config, global_config );
    read_compute_limit                = util::get_option< uint64_t >( constants::read_compute_bandwidth_limit_option, constants::read_compute_bandwidth_limit_default, args, chain_config, global_config );
    fork_algorithm_option             = util::get_option< std::string >( constants::fork_algorithm_option, constants::fork_algorithm_default, args, chain_config, global_config );
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

    if( fork_algorithm_option == constants::fifo_algorithm )
    {
      LOG( info ) << "Using fork resolution algorithm: " << constants::fifo_algorithm;
      fork_algorithm = state_db::fork_resolution_algorithm::fifo;
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
    attrs.set_stack_size( constants::eight_megabytes );

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
