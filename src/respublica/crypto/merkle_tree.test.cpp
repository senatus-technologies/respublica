// NOLINTBEGIN

#include <cstddef>
#include <gtest/gtest.h>
#include <iomanip>
#include <respublica/crypto/merkle_tree.hpp>
#include <span>

template< typename Blob >
std::string hex_string( const Blob& b )
{
  std::stringstream ss;
  ss << std::hex;

  for( int i = 0; i < b.size(); ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << (int)b.at( i );

  return ss.str();
}

TEST( merkle_root, root )
{
  std::vector< std::string > values1{ "the", "quick", "brown", "fox", "jumps", "over", "a", "lazy", "dog" };
  auto tree1 = respublica::crypto::merkle_tree< std::string >::create( values1 );
  EXPECT_EQ( hex_string( tree1.root()->hash() ), "677f12631490263cce295f42b52eb9066f0c7e8d1845ca3fd80a2e52ab2db29e" );
  EXPECT_EQ( tree1.root()->hash(), respublica::crypto::merkle_root( values1 ) );

  std::vector< respublica::crypto::digest > values2{ respublica::crypto::hash( "the" ),
                                                     respublica::crypto::hash( "quick" ),
                                                     respublica::crypto::hash( "brown" ),
                                                     respublica::crypto::hash( "fox" ),
                                                     respublica::crypto::hash( "jumps" ),
                                                     respublica::crypto::hash( "over" ),
                                                     respublica::crypto::hash( "a" ),
                                                     respublica::crypto::hash( "lazy" ),
                                                     respublica::crypto::hash( "dog" ) };
  auto tree2 = respublica::crypto::merkle_tree< respublica::crypto::digest, true >::create( values2 );

  EXPECT_EQ( hex_string( tree2.root()->hash() ), "677f12631490263cce295f42b52eb9066f0c7e8d1845ca3fd80a2e52ab2db29e" );
  EXPECT_EQ( tree2.root()->hash(), respublica::crypto::merkle_root< true >( values2 ) );

  std::vector< std::span< const std::byte > > values3;
  values3.reserve( values1.size() );

  for( const auto& value: values1 )
    values3.push_back( std::as_bytes( std::span( value ) ) );

  auto tree3 = respublica::crypto::merkle_tree< std::span< const std::byte > >::create( values3 );
  EXPECT_EQ( hex_string( tree3.root()->hash() ), "677f12631490263cce295f42b52eb9066f0c7e8d1845ca3fd80a2e52ab2db29e" );
  EXPECT_EQ( tree3.root()->hash(), respublica::crypto::merkle_root( values3 ) );
}

// NOLINTEND
