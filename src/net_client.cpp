#include <iostream>
#include <print>

#include <boost/algorithm/string.hpp>
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
    std::vector< std::string > endpoint_parts;
    boost::split( endpoint_parts, args[ "endpoint" ].as< std::string >(), boost::is_any_of( ":" ) );
    std::println( "Endpoint IP: {}, Port: {}", endpoint_parts[ 0 ], endpoint_parts[ 1 ] );
    boost::asio::ip::tcp::resolver resolver( ioc );
    endpoints = resolver.resolve( endpoint_parts[ 0 ], endpoint_parts[ 1 ] );
  }

  respublica::net::client client( ioc, port, endpoints );
  ioc.run();
  return EXIT_SUCCESS;
}
