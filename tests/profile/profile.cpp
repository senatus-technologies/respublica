// NOLINTBEGIN

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>

#include <test/fixture.hpp>

static std::unique_ptr< test::fixture > fixture;

static koinos::protocol::transaction coin_tx;
static koinos::protocol::transaction token_tx;

static bool setup()
{
  auto alice_secret_key = koinos::crypto::secret_key::create( koinos::crypto::hash( "alice" ) );
  auto bob_secret_key   = koinos::crypto::secret_key::create( koinos::crypto::hash( "bob" ) );
  auto token_secret_key = koinos::crypto::secret_key::create( koinos::crypto::hash( "token" ) );

  token_tx = fixture->make_transaction(
    alice_secret_key,
    1,
    1'000'000,
    fixture->make_transfer_operation( koinos::protocol::program_account( token_secret_key.public_key().bytes() ),
                                      koinos::protocol::user_account( alice_secret_key.public_key().bytes() ),
                                      koinos::protocol::user_account( bob_secret_key.public_key().bytes() ),
                                      0 ) );

  coin_tx = fixture->make_transaction(
    alice_secret_key,
    1,
    1'000'000,
    fixture->make_transfer_operation( koinos::protocol::system_program( "coin" ),
                                      koinos::protocol::user_account( alice_secret_key.public_key().bytes() ),
                                      koinos::protocol::user_account( bob_secret_key.public_key().bytes() ),
                                      0 ) );

  koinos::protocol::block block = fixture->make_block(
    fixture->_block_signing_secret_key,
    fixture->make_transaction( token_secret_key,
                               1,
                               10'000'000,
                               fixture->make_upload_program_operation(
                                 koinos::protocol::program_account( token_secret_key.public_key().bytes() ),
                                 koin_program() ) ) );

  return fixture->verify( fixture->_controller->process( block ),
                          test::fixture::verification::head | test::fixture::verification::without_reversion );
}

constexpr std::string_view token_cpu     = "token.cpu";
constexpr std::string_view token_heap    = "token.heap";
constexpr std::string_view coin_cpu      = "coin.cpu";
constexpr std::string_view coin_heap     = "coin.heap";
constexpr std::string_view requests_cpu  = "requests.cpu";
constexpr std::string_view requests_heap = "requests.heap";

int main( int argc, char** argv )
{
  boost::program_options::variables_map vm;
  std::vector< std::string_view > valid_profiles =
    { token_cpu, token_heap, coin_cpu, coin_heap, requests_cpu, requests_heap };

  try
  {
    // clang-format off
    boost::program_options::options_description desc{ "Options" };
    desc.add_options()
      ( "help,h", "Help screen" )
      ( "alert,a",
        boost::program_options::bool_switch()->default_value(false),
        "Alerts upon completion" )
      ( "jobs,j",
        boost::program_options::value< std::uint64_t >()->default_value( boost::thread::hardware_concurrency() ),
        "Hardware concurrency" )
      ( "iterations,i",
        boost::program_options::value< std::uint64_t >()->default_value( 100'000),
        "Number of iterations per profile" )
      ( "profiles,p",
        boost::program_options::value< std::vector< std::string > >()->multitoken(),
         "Profiles to run, leave blank for all profiles, valid options:\n"
         "* coin.cpu\n* coin.heap\n* token.cpu\n* token.heap\n* requests.cpu\n* requests.heap" );
    // clang-format on

    boost::program_options::store( boost::program_options::parse_command_line( argc, argv, desc ), vm );
    boost::program_options::notify( vm );

    if( vm.count( "help" ) )
    {
      std::cout << desc << '\n';
      return EXIT_SUCCESS;
    }

    if( vm.count( "profiles" ) )
    {
      for( const auto& profile: vm[ "profiles" ].as< std::vector< std::string > >() )
      {
        if( auto it = std::find( valid_profiles.begin(), valid_profiles.end(), profile ); it == valid_profiles.end() )
        {
          std::cerr << "Invalid profile: " << profile << '\n';
          return EXIT_FAILURE;
        }
      }
    }

    if( vm.count( "jobs" ) )
    {
      if( vm[ "jobs" ].as< std::uint64_t >() < 1 )
      {
        std::cerr << "Cannot run profile(s) with less than 1 job\n";
        return EXIT_FAILURE;
      }
    }
  }
  catch( const boost::program_options::error& ex )
  {
    std::cerr << ex.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "Profile configuration:\n";

  std::cout << "  Jobs: " << vm[ "jobs" ].as< std::uint64_t >() << '\n';

  if( vm.count( "profiles" ) )
  {
    std::cout << "  Profiles: ";
    for( const auto& profile: vm[ "profiles" ].as< std::vector< std::string > >() )
      std::cout << profile << " ";
    std::cout << '\n';
  }
  else
  {
    std::cout << "  Profiles: All\n";
  }

  std::cout << "  Iterations: " << vm[ "iterations" ].as< std::uint64_t >() << '\n';

  const char* sampling_frequency = std::getenv( "CPUPROFILE_FREQUENCY" );
  if( !sampling_frequency )
  {
    std::cout << "  Sampling frequency: Default" << '\n';
    std::cout << "** Set the sampling frequency by running with the environment variables CPUPROFILE_FREQUENCY **"
              << '\n';
    std::cout << "  e.g. CPUPROFILE_FREQUENCY=10000 " << argv[ 0 ] << '\n';
  }
  else
  {
    std::cout << "  Sampling frequency: " << sampling_frequency << '\n';
  }

  boost::asio::thread_pool pool( vm[ "jobs" ].as< std::uint64_t >() );

  const bool with_pool = vm[ "jobs" ].as< std::uint64_t >() > 1;

  fixture = std::make_unique< test::fixture >( "profile", "info" );

  if( !setup() )
    return EXIT_FAILURE;

  std::vector< std::string > profiles;

  if( vm.count( "profiles" ) )
    profiles = vm[ "profiles" ].as< std::vector< std::string > >();

  std::uint64_t iterations = vm[ "iterations" ].as< std::uint64_t >();

  auto it = std::find( profiles.begin(), profiles.end(), token_cpu );
  if( profiles.empty() || it != profiles.end() )
  {
    std::cout << "[Profile]: " << token_cpu << "\n";
    if( with_pool )
    {
      ProfilerStart( "token.transactions.cpu.out" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        boost::asio::post( pool,
                           []()
                           {
                             [[maybe_unused]]
                             auto response = fixture->_controller->process( token_tx );
                           } );
      }
      pool.join();
      ProfilerStop();
    }
    else
    {
      ProfilerStart( "token.transactions.cpu.out" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        [[maybe_unused]]
        auto response = fixture->_controller->process( token_tx );
      }
      ProfilerStop();
    }
  }

  it = std::find( profiles.begin(), profiles.end(), token_heap );
  if( profiles.empty() || it != profiles.end() )
  {
    std::cout << "[Profile]: " << token_heap << "\n";
    if( with_pool )
    {
      HeapProfilerStart( "token.transactions" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        boost::asio::post( pool,
                           []()
                           {
                             [[maybe_unused]]
                             auto response = fixture->_controller->process( token_tx );
                           } );
      }
      pool.join();
      HeapProfilerStop();
    }
    else
    {
      HeapProfilerStart( "token.transactions" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        [[maybe_unused]]
        auto response = fixture->_controller->process( token_tx );
      }
      HeapProfilerStop();
    }
  }

  it = std::find( profiles.begin(), profiles.end(), coin_cpu );
  if( profiles.empty() || it != profiles.end() )
  {
    std::cout << "[Profile]: " << coin_cpu << "\n";
    if( with_pool )
    {
      ProfilerStart( "coin.transactions.cpu.out" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        boost::asio::post( pool,
                           []()
                           {
                             [[maybe_unused]]
                             auto response = fixture->_controller->process( coin_tx );
                           } );
      }
      pool.join();
      ProfilerStop();
    }
    else
    {
      ProfilerStart( "coin.transactions.cpu.out" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        [[maybe_unused]]
        auto response = fixture->_controller->process( coin_tx );
      }
      ProfilerStop();
    }
  }

  it = std::find( profiles.begin(), profiles.end(), coin_heap );
  if( profiles.empty() || it != profiles.end() )
  {
    std::cout << "[Profile]: " << coin_heap << "\n";
    if( with_pool )
    {
      HeapProfilerStart( "coin.transactions" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        boost::asio::post( pool,
                           []()
                           {
                             [[maybe_unused]]
                             auto response = fixture->_controller->process( coin_tx );
                           } );
      }
      pool.join();
      HeapProfilerStop();
    }
    else
    {
      HeapProfilerStart( "coin.transactions" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        [[maybe_unused]]
        auto response = fixture->_controller->process( coin_tx );
      }
      HeapProfilerStop();
    }
  }

  it = std::find( profiles.begin(), profiles.end(), requests_cpu );
  if( profiles.empty() || it != profiles.end() )
  {
    std::cout << "[Profile]: " << requests_cpu << "\n";
    if( with_pool )
    {
      ProfilerStart( "requests.cpu.out" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        boost::asio::post( pool,
                           []()
                           {
                             [[maybe_unused]]
                             auto response = fixture->_controller->head();
                           } );
      }
      pool.join();
      ProfilerStop();
    }
    else
    {
      ProfilerStart( "requests.cpu.out" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        [[maybe_unused]]
        auto response = fixture->_controller->head();
      }
      ProfilerStop();
    }
  }

  it = std::find( profiles.begin(), profiles.end(), requests_heap );
  if( profiles.empty() || it != profiles.end() )
  {
    std::cout << "[Profile]: " << requests_heap << "\n";
    if( with_pool )
    {
      HeapProfilerStart( "requests" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        boost::asio::post( pool,
                           []()
                           {
                             [[maybe_unused]]
                             auto response = fixture->_controller->head();
                           } );
      }
      pool.join();
      HeapProfilerStop();
    }
    else
    {
      HeapProfilerStart( "requests" );
      for( std::uint64_t i = 0; i < iterations; i++ )
      {
        [[maybe_unused]]
        auto response = fixture->_controller->head();
      }
      HeapProfilerStop();
    }
  }

  fixture = nullptr;

  std::cout << "Profiling complete, view results by invoking pprof\n";
  std::cout << "  e.g. pprof --text " << argv[ 0 ] << " <output_file>\n";

  if( vm[ "alert" ].as< bool >() )
    std::cout << '\a';

  return EXIT_SUCCESS;
}

// NOLINTEND
