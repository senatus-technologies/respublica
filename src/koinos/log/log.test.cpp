#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include <koinos/log/log.hpp>

TEST( log, color )
{
  std::stringbuf buffer;
  std::streambuf* clog_buffer;
  buffer.str( "" );
  clog_buffer = std::clog.rdbuf();

  auto buf = std::clog.rdbuf( &buffer );

  std::vector< std::string > logtypes{ "\033[34mtrace\033[0m",
                                       "\033[34mdebug\033[0m",
                                       "\033[32minfo\033[0m ",
                                       "\033[33mwarn\033[0m ",
                                       "\033[31merror\033[0m",
                                       "\033[31mfatal\033[0m" };

  koinos::log::initialize( "color", "trace", {}, true );

  LOG( trace ) << "test";
  LOG( debug ) << "test";
  LOG( info ) << "test";
  LOG( warning ) << "test";
  LOG( error ) << "test";
  LOG( fatal ) << "test";

  std::vector< std::string > results;
  std::string stream_str = buffer.str();
  boost::split( results, stream_str, boost::is_any_of( "\n" ) );
  results.pop_back();

  std::clog.rdbuf( buf );

  EXPECT_EQ( results.size(), logtypes.size() );
  for( int i = 0; i < results.size(); i++ )
  {
    std::string expected_result = results[ i ].substr( 0, 14 );
    EXPECT_EQ( logtypes[ i ], expected_result );
  }

  boost::log::core::get()->remove_all_sinks();
  std::clog.rdbuf( clog_buffer );
}

TEST( log, no_color )
{
  std::stringbuf buffer;
  std::streambuf* clog_buffer;
  buffer.str( "" );
  clog_buffer = std::clog.rdbuf();

  auto buf = std::clog.rdbuf( &buffer );

  std::vector< std::string > logtypes{ "trace", "debug", "info ", "warn ", "error", "fatal" };

  koinos::log::initialize( "color", "trace", {}, false );

  LOG( trace ) << "test";
  LOG( debug ) << "test";
  LOG( info ) << "test";
  LOG( warning ) << "test";
  LOG( error ) << "test";
  LOG( fatal ) << "test";

  std::vector< std::string > results;
  std::string stream_str = buffer.str();
  boost::split( results, stream_str, boost::is_any_of( "\n" ) );
  results.pop_back();

  std::clog.rdbuf( buf );

  EXPECT_EQ( results.size(), logtypes.size() );
  for( int i = 0; i < results.size(); i++ )
  {
    std::string expected_result = results[ i ].substr( 0, 5 );
    EXPECT_EQ( logtypes[ i ], expected_result );
  }

  boost::log::core::get()->remove_all_sinks();
  std::clog.rdbuf( clog_buffer );
}

TEST( log, filter )
{
  std::stringbuf buffer;
  std::streambuf* clog_buffer;
  buffer.str( "" );
  clog_buffer = std::clog.rdbuf();

  auto buf = std::clog.rdbuf( &buffer );

  std::vector< std::string > logtypes{ "warn ", "error", "fatal" };

  koinos::log::initialize( "color", "warn", {}, false );

  LOG( trace ) << "test";
  LOG( debug ) << "test";
  LOG( info ) << "test";
  LOG( warning ) << "test";
  LOG( error ) << "test";
  LOG( fatal ) << "test";

  std::vector< std::string > results;
  std::string stream_str = buffer.str();
  boost::split( results, stream_str, boost::is_any_of( "\n" ) );
  results.pop_back();

  std::clog.rdbuf( buf );

  EXPECT_EQ( results.size(), logtypes.size() );
  for( int i = 0; i < results.size(); i++ )
  {
    std::string expected_result = results[ i ].substr( 0, 5 );
    EXPECT_EQ( logtypes[ i ], expected_result );
  }

  boost::log::core::get()->remove_all_sinks();
  std::clog.rdbuf( clog_buffer );
}
