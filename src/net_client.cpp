#include <iostream>
#include <print>

#include <boost/program_options.hpp>

#include <respublica/net.hpp>

constexpr unsigned short default_port = 43'333;

auto main( int argc, char** argv ) -> int
{
  boost::program_options::options_description options;

  // clang-format off
  options.add_options()
    ( "help,h"    , "Print this help message and exit" )
    ( "version,v" , "Print version string and exit" )
    ( "port,p"    , boost::program_options::value< std::uint16_t >()->default_value( default_port ), "Default listening port" )
    ( "endpoint,e", boost::program_options::value< std::string >(), "The endpoint to connect to");
  // clang-format on

  boost::program_options::variables_map args;
  boost::program_options::store( boost::program_options::parse_command_line( argc, argv, options ), args );
  boost::program_options::notify( args );

  std::uint16_t port = 0;
  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver::results_type endpoints;

  if( args.count( "help" ) )
  {
    options.print( std::cout );
    return EXIT_SUCCESS;
  }

  if( args.count( "version" ) )
  {
    std::println( "v0.0.1" );
    return EXIT_SUCCESS;
  }

  if( !args.count( "port" ) )
  {
    std::println( "Port is required." );
    return EXIT_FAILURE;
  }
  port = args[ "port" ].as< std::uint16_t >();

  if( args.count( "endpoint" ) )
  {
    const std::string endpoint_str = args[ "endpoint" ].as< std::string >();
    const auto colon_pos           = endpoint_str.rfind( ':' );

    if( colon_pos == std::string::npos )
    {
      std::println( "Invalid endpoint format. Expected: <host>:<port> (e.g., 192.168.1.1:8080 or localhost:8080)" );
      return EXIT_FAILURE;
    }

    const std::string host     = endpoint_str.substr( 0, colon_pos );
    const std::string port_str = endpoint_str.substr( colon_pos + 1 );

    try
    {
      boost::asio::ip::tcp::resolver resolver( ioc );
      endpoints = resolver.resolve( host, port_str );
      std::println( "Endpoint host: {}, Port: {}", host, port_str );
    }
    catch( const boost::system::system_error& e )
    {
      std::println( "Invalid endpoint '{}': {}", endpoint_str, e.what() );
      return EXIT_FAILURE;
    }
  }

  respublica::net::client client( ioc, port, endpoints );
  ioc.run();
  return EXIT_SUCCESS;
}
