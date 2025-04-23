#include <gtest/gtest.h>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/util/hex.hpp>

template< typename Blob >
std::string hex_string( const Blob& b )
{
  std::stringstream ss;
  ss << std::hex;

  for( int i = 0; i < b.size(); ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << (int)b[ i ];

  return ss.str();
}

TEST( merkle_tree, root )
{
  std::vector< std::string > values{ "the", "quick", "brown", "fox", "jumps", "over", "a", "lazy", "dog" };

  std::vector< std::string > wh_hex{ "b9776d7ddf459c9ad5b0e1d6ac61e27befb5e99fd62446677600d7cacef544d0",
                                     "22c72aa82ce77c82e2ca65a711c79eaa4b51c57f85f91489ceeacc7b385943ba",
                                     "5eb67f9f8409b9c3f739735633cbdf92121393d0e13bd0f464b1b2a6a15ad2dc",
                                     "776cb326ab0cd5f0a974c1b9606044d8485201f2db19cf8e3749bdee5f36e200",
                                     "ef30940a2d1b943c8007b8a15e45935dc01902b7c0534dc7e27fda30a9b81aef",
                                     "5fb6a47e368e12e5d8b19280796e6a3d146fe391ed2e967d5f95c55bfb0f9c2f",
                                     "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb",
                                     "81fd67d02f679b818a4df6a50139958aa857eddc4d8f3561630dfb905e6d3c24",
                                     "cd6357efdd966de8c0cb2f876cc89ec74ce35f0968e11743987084bd42fb8944" };

  auto h = [ & ]( const koinos::crypto::multihash& ha,
                  const koinos::crypto::multihash& hb ) -> koinos::crypto::multihash
  {
    std::vector< std::byte > temp;
    std::copy( ha.digest().begin(), ha.digest().end(), std::back_inserter( temp ) );
    std::copy( hb.digest().begin(), hb.digest().end(), std::back_inserter( temp ) );
    koinos::crypto::multihash result = *hash( koinos::crypto::multicodec::sha2_256, (char*)temp.data(), temp.size() );
    return result;
  };

  // Hash of each word
  std::vector< koinos::crypto::multihash > wh;
  for( size_t i = 0; i < values.size(); i++ )
  {
    wh.push_back( *hash( koinos::crypto::multicodec::sha2_256, values[ i ].c_str(), values[ i ].size() ) );
    EXPECT_EQ( wh_hex[ i ], hex_string( wh[ i ].digest() ) );
  }

  const std::string n01        = "0020397085ab4494829e691c49353a04d3201fda20c6a8a6866cf0f84bb8ce47";
  const std::string n23        = "78d4e37706320c82b2dd092eeb04b1f271523f86f910bf680ff9afcb2f8a33e1";
  const std::string n0123      = "e07aa684d91ffcbb89952f5e99b6181f7ee7bd88bd97be1345fc508f1062c050";
  const std::string n45        = "4185f41c5d7980ae7d14ce248f50e2854826c383671cf1ee3825ea957315c627";
  const std::string n67        = "b2a6704395c45ad8c99247103b580f7e7a37f06c3d38075ce4b02bc34c6a6754";
  const std::string n4567      = "2f24a249901ee8392ba0bb3b90c8efd6e2fee6530f45769199ef82d0b091d8ba";
  const std::string n01234567  = "913b7dce068efc8db6fab0173481f137ce91352b341855a1719aaff926169987";
  const std::string n8         = "cd6357efdd966de8c0cb2f876cc89ec74ce35f0968e11743987084bd42fb8944";
  const std::string n012345678 = "e24e552e0b6cf8835af179a14a766fb58c23e4ee1f7c6317d57ce39cc578cfac";

  koinos::crypto::multihash h01        = h( wh[ 0 ], wh[ 1 ] );
  koinos::crypto::multihash h23        = h( wh[ 2 ], wh[ 3 ] );
  koinos::crypto::multihash h0123      = h( h01, h23 );
  koinos::crypto::multihash h45        = h( wh[ 4 ], wh[ 5 ] );
  koinos::crypto::multihash h67        = h( wh[ 6 ], wh[ 7 ] );
  koinos::crypto::multihash h4567      = h( h45, h67 );
  koinos::crypto::multihash h01234567  = h( h0123, h4567 );
  koinos::crypto::multihash h8         = wh[ 8 ];
  koinos::crypto::multihash h012345678 = h( h01234567, h8 );

  EXPECT_EQ( n01, hex_string( h01.digest() ) );
  EXPECT_EQ( n23, hex_string( h23.digest() ) );
  EXPECT_EQ( n0123, hex_string( h0123.digest() ) );
  EXPECT_EQ( n45, hex_string( h45.digest() ) );
  EXPECT_EQ( n67, hex_string( h67.digest() ) );
  EXPECT_EQ( n4567, hex_string( h4567.digest() ) );
  EXPECT_EQ( n01234567, hex_string( h01234567.digest() ) );
  EXPECT_EQ( n012345678, hex_string( h012345678.digest() ) );

  auto tree = koinos::crypto::merkle_tree< std::string >::create( koinos::crypto::multicodec::sha2_256, values );
  if( !tree )
    FAIL();
  else
  {
    EXPECT_EQ( n012345678, hex_string( tree->root()->hash().digest() ) );

    EXPECT_EQ( *tree->root()->left()->left()->left()->left()->value(), values[ 0 ] );    // the
    EXPECT_EQ( *tree->root()->left()->left()->left()->right()->value(), values[ 1 ] );   // quick
    EXPECT_EQ( *tree->root()->left()->left()->right()->left()->value(), values[ 2 ] );   // brown
    EXPECT_EQ( *tree->root()->left()->left()->right()->right()->value(), values[ 3 ] );  // fox
    EXPECT_EQ( *tree->root()->left()->right()->left()->left()->value(), values[ 4 ] );   // jumps
    EXPECT_EQ( *tree->root()->left()->right()->left()->right()->value(), values[ 5 ] );  // over
    EXPECT_EQ( *tree->root()->left()->right()->right()->left()->value(), values[ 6 ] );  // a
    EXPECT_EQ( *tree->root()->left()->right()->right()->right()->value(), values[ 7 ] ); // lazy
    EXPECT_EQ( *tree->root()->right()->value(), values[ 8 ] );                           // dog

    std::vector< koinos::crypto::multihash > v( values.size() );
    std::transform( std::begin( values ),
                    std::end( values ),
                    std::begin( v ),
                    []( const std::string& s )
                    {
                      return *hash( koinos::crypto::multicodec::sha2_256, s );
                    } );

    auto multihash_tree = koinos::crypto::merkle_tree< koinos::crypto::multihash >::create( koinos::crypto::multicodec::sha2_256, v );
    if( !multihash_tree )
      FAIL();
    else
      EXPECT_EQ( multihash_tree->root()->hash(), tree->root()->hash() );
  }

  auto mtree = koinos::crypto::merkle_tree< std::string >::create( koinos::crypto::multicodec::sha2_256, std::vector< std::string >() );
  if( !mtree )
    FAIL();
  else
  {
    EXPECT_EQ( mtree->root()->hash(), koinos::crypto::multihash::empty( koinos::crypto::multicodec::sha2_256 ) );
    EXPECT_NE( mtree->root()->hash(), koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) );
  }
}
