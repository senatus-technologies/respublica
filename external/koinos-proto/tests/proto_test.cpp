#include <boost/test/unit_test.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <koinos/block_store/block_store.pb.h>
#include <koinos/common.pb.h>
#include <koinos/serialization.hpp>
#include <koinos/test_map.pb.h>

struct proto_fixture
{};

BOOST_FIXTURE_TEST_SUITE( proto_tests, proto_fixture )

BOOST_AUTO_TEST_CASE( canonical_test )
{
  BOOST_TEST_MESSAGE( "Testing canonical serialization" );

  // {"id": "foo", "height": 1, "previous": "bar"}
  constexpr char canon_cstr[] = { 0x0A, 0x03, 0x66, 0x6F, 0x6F, 0x10, 0x01, 0x1a, 0x03, 0x62, 0x61, 0x72 };
  std::string canon( canon_cstr, sizeof( canon_cstr ) );

  koinos::block_topology topo;
  topo.set_id( "foo" );
  topo.set_height( 1 );
  topo.set_previous( "bar" );

  BOOST_CHECK( serialize_canonically( topo ) == canon );

  std::stringstream ss;
  serialize_canonically( ss, topo );

  BOOST_CHECK( ss.str() == canon );

  koinos::test_map tm;
  auto m      = tm.mutable_im_the_map();
  ( *m )[ 0 ] = "foo";
  ( *m )[ 1 ] = "bar";

  try
  {
    auto ser = serialize_canonically( tm );
    BOOST_CHECK( false );
  }
  catch( std::runtime_error& )
  {}

  try
  {
    std::stringstream ss;
    serialize_canonically( ss, tm );
    BOOST_CHECK( false );
  }
  catch( std::runtime_error& )
  {}

  koinos::map_wrapper mw;
  m           = mw.mutable_nested_map()->mutable_im_the_map();
  ( *m )[ 0 ] = "foo";
  ( *m )[ 1 ] = "bar";

  try
  {
    auto ser = serialize_canonically( mw );
    BOOST_CHECK( false );
  }
  catch( std::runtime_error& )
  {}

  try
  {
    std::stringstream ss;
    serialize_canonically( ss, mw );
    BOOST_CHECK( false );
  }
  catch( std::runtime_error& )
  {}
}

std::string to_hex( const std::string& s )
{
  std::stringstream stream;
  stream << "0x" << std::hex << std::setfill( '0' );
  for( const auto& c: s )
  {
    stream << std::setw( 2 ) << static_cast< unsigned int >( static_cast< unsigned char >( c ) );
  }
  return stream.str();
}

BOOST_AUTO_TEST_CASE( unknown_field_test )
{
  koinos::block_store::block_item bi;
  koinos::block_store::block_record br;

  br.set_block_id( std::string( "123" ) );
  br.set_block_height( 123 );

  koinos::protocol::block b;
  b.set_id( std::string( "456" ) );

  koinos::protocol::block_receipt r;
  r.set_id( std::string( "456" ) );

  *br.mutable_block()   = b;
  *br.mutable_receipt() = r;

  *br.add_previous_block_ids() = std::string( "789" );
  *br.add_previous_block_ids() = std::string( "101112" );

  bi.ParseFromString( koinos::serialize_canonically( br ) );

  BOOST_REQUIRE_EQUAL( koinos::serialize_canonically( bi ), koinos::serialize_canonically( br ) );

  // Put the repeated fields first and then check canonical serialization
  constexpr char noncanon_cstr[] = { 0x2a, 0x03, 0x37, 0x38, 0x39, 0x2a, 0x06, 0x31, 0x30, 0x31, 0x31, 0x31,
                                     0x32, 0x0a, 0x03, 0x31, 0x32, 0x33, 0x10, 0x7b, 0x1a, 0x05, 0x0a, 0x03,
                                     0x34, 0x35, 0x36, 0x22, 0x05, 0x0a, 0x03, 0x34, 0x35, 0x36 };

  koinos::block_store::block_item bi2;
  bi2.ParseFromArray( noncanon_cstr, sizeof( noncanon_cstr ) );

  BOOST_CHECK_EQUAL( koinos::serialize_canonically( bi2 ), koinos::serialize_canonically( br ) );
}

BOOST_AUTO_TEST_SUITE_END()
