#include <boost/test/unit_test.hpp>

#include <koinos/exception.hpp>

#include <iostream>

#include "test_object.pb.h"

struct exception_fixture
{};

struct simple_struct
{
  int x = 0;

  friend std::ostream& operator<<( std::ostream&, const simple_struct& );
};

std::ostream& operator<<( std::ostream& out, const simple_struct& s )
{
  return out << s.x;
}

KOINOS_DECLARE_EXCEPTION( basic_exception );
KOINOS_DECLARE_EXCEPTION_WITH_CODE( code_exception, 12 );
KOINOS_DECLARE_DERIVED_EXCEPTION( derived_exception, code_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( derived_code_exception, derived_exception, 88 );

BOOST_FIXTURE_TEST_SUITE( exception_tests, exception_fixture )

template< typename E >
void test_exception( uint32_t expected_code )
{
  nlohmann::json exception_json;
  exception_json[ "x" ] = "foo";
  exception_json[ "y" ] = "bar";

  BOOST_TEST_MESSAGE( "Throw an exception with an initial capture and a caught capture." );
  try
  {
    try
    {
      KOINOS_THROW( E, "exception_test ${x} ${y}", ( "x", "foo" ) );
    }
    KOINOS_CAPTURE_CATCH_AND_RETHROW( ( "y", "bar" ) )
  }
  catch( koinos::exception& e )
  {
    auto j = e.get_json();
    BOOST_REQUIRE_EQUAL( exception_json, j );
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test foo bar" );
    BOOST_REQUIRE_EQUAL( e.what(), e.get_message() );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with no initial capture and a caught capture." );
  try
  {
    try
    {
      KOINOS_THROW( E, "exception_test ${x} ${y}" );
    }
    KOINOS_CAPTURE_CATCH_AND_RETHROW( ( "y", "bar" )( "x", "foo" ) )
  }
  catch( koinos::exception& e )
  {
    auto j = e.get_json();
    BOOST_REQUIRE_EQUAL( exception_json, j );
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test foo bar" );
    BOOST_REQUIRE_EQUAL( e.what(), e.get_message() );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an initial capture and a caught extra capture." );
  try
  {
    try
    {
      KOINOS_THROW( E, "exception_test ${x} ${y}", ( "y", "bar" )( "x", "foo" ) );
    }
    KOINOS_CAPTURE_CATCH_AND_RETHROW( ( "z", 10 ) )
  }
  catch( koinos::exception& e )
  {
    exception_json[ "z" ] = 10;
    auto j                = e.get_json();
    BOOST_REQUIRE_EQUAL( exception_json, j );
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test foo bar" );
    BOOST_REQUIRE_EQUAL( e.what(), e.get_message() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an initial object capture and a missing capture." );
  try
  {
    test_object outer_obj;
    outer_obj.set_x( 3 );
    outer_obj.set_y( 4 );

    try
    {
      test_object inner_obj;
      inner_obj.set_x( 1 );
      inner_obj.set_y( 2 );

      KOINOS_THROW( E, "exception_test ${x} ${y}", ( "x", inner_obj ) );
    }
    KOINOS_CAPTURE_CATCH_AND_RETHROW( ( "z", outer_obj ) )
  }
  catch( koinos::exception& e )
  {
    exception_json.clear();
    exception_json[ "x" ][ "x" ] = 1;
    exception_json[ "x" ][ "y" ] = 2;
    exception_json[ "z" ][ "x" ] = 3;
    exception_json[ "z" ][ "y" ] = 4;
    auto j                       = e.get_json();
    BOOST_REQUIRE_EQUAL( exception_json, j );
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test {\"x\":1,\"y\":2} ${y}" );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an object capture." );
  try
  {
    test_object obj;
    obj.set_x( 1 );
    obj.set_y( 2 );

    KOINOS_THROW( E, "exception_test ${x} ${y}", ( "x", obj ) );
  }
  catch( koinos::exception& e )
  {
    exception_json.clear();
    exception_json[ "x" ][ "x" ] = 1;
    exception_json[ "x" ][ "y" ] = 2;
    auto j                       = e.get_json();
    BOOST_REQUIRE_EQUAL( exception_json, j );
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test {\"x\":1,\"y\":2} ${y}" );
    BOOST_REQUIRE_EQUAL( e.get_message(), e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an initial implicit const object capture." );
  try
  {
    test_object obj;
    obj.set_x( 1 );
    obj.set_y( 2 );

    KOINOS_THROW( E, "exception_test ${x} ${y}", ( obj ) );
  }
  catch( koinos::exception& e )
  {
    exception_json.clear();
    exception_json[ "x" ] = 1;
    exception_json[ "y" ] = 2;
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test 1 2" );
    BOOST_REQUIRE_EQUAL( e.get_message(), e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an object with a stream operator." );
  try
  {
    simple_struct obj = { 1 };

    KOINOS_THROW( E, "exception_test ${x}", ( "x", obj ) );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test 1" );
    BOOST_REQUIRE_EQUAL( e.get_message(), e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with a message that has been moved." );
  try
  {
    std::string msg = "moved exception message";
    throw koinos::exception( std::move( msg ) );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( "moved exception message", e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), 1 );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an escaped message." );
  try
  {
    std::string msg = "An escaped message ${$escaped}";
    KOINOS_THROW( E, std::move( msg ), ( "escaped", 1 ) );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( "An escaped message ${$escaped}", e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with an embedded dollar sign." );
  try
  {
    std::string msg = "A dollar signed $ within a message";
    KOINOS_THROW( E, std::move( msg ), ()( "x", 1 ) );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( "A dollar signed $ within a message", e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with a std::size_t replacement." );
  try
  {
    std::string msg = "My std::size_t value is ${s}";
    KOINOS_THROW( E, std::move( msg ), ( "s", std::size_t( 20 ) ) );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( "My std::size_t value is 20", e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception and test for the existence of a stacktrace." );
  try
  {
    KOINOS_THROW( E, "An exception that should contain a stacktrace" );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE( e.get_stacktrace().size() > 0 );
  }

  BOOST_TEST_MESSAGE( "Throw an exception with a malformed token." );
  try
  {
    KOINOS_THROW( E, "An exception ${with a malformed token", ( "w", 1 ) );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( "An exception ${with a malformed token", e.what() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception without using the provided macros." );
  try
  {
    throw E( "An exception" );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( e.get_json(), nlohmann::json() );
    BOOST_REQUIRE_EQUAL( e.get_stacktrace(), std::string() );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Throw an exception adding JSON via add_json API." );
  try
  {
    KOINOS_THROW( E, "An exception ${string}" );
  }
  catch( koinos::exception& e )
  {
    BOOST_REQUIRE_EQUAL( e.get_json(), nlohmann::json() );
    BOOST_REQUIRE( e.get_stacktrace().size() > 0 );

    std::vector< std::string > vec{ "alpha", "bravo", "charlie" };
    e.add_json( "things", vec );

    nlohmann::json j_array = nlohmann::json::array();
    j_array.push_back( "alpha" );
    j_array.push_back( "bravo" );
    j_array.push_back( "charlie" );
    BOOST_REQUIRE_EQUAL( e.get_json()[ "things" ], j_array );

    e.add_json( "string", "delta" );
    BOOST_REQUIRE_EQUAL( e.get_json()[ "string" ], "delta" );

    BOOST_REQUIRE_EQUAL( e.what(), "An exception delta" );
    BOOST_REQUIRE_EQUAL( e.get_code(), expected_code );
  }

  BOOST_TEST_MESSAGE( "Check throwing an exception with error_data" );
  koinos::chain::error_data error;
  error.set_message( "test message" );

  try
  {
    throw E( error );
  }
  catch( koinos::exception& e )
  {
    BOOST_CHECK_EQUAL( e.get_message(), error.message() );
    BOOST_CHECK_EQUAL( e.get_data().message(), error.message() );
  }
}

BOOST_AUTO_TEST_CASE( exception_test )
{
  try
  {
    test_exception< basic_exception >( 1 );
    test_exception< code_exception >( 12 );
    test_exception< derived_exception >( 12 );
    test_exception< derived_code_exception >( 88 );
  }
  KOINOS_CATCH_LOG_AND_RETHROW( info )
}

BOOST_AUTO_TEST_SUITE_END()
