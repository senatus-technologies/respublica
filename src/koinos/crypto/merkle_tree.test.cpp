#include <cstddef>
#include <gtest/gtest.h>
#include <iomanip>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/util/hex.hpp>
#include <span>

template< typename Blob >
std::string hex_string( const Blob& b )
{
  std::stringstream ss;
  ss << std::hex;

  for( int i = 0; i < b.size(); ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << (int)b[ i ];

  return ss.str();
}

TEST( merkle_root, root )
{
  std::vector< std::string > values1{ "the", "quick", "brown", "fox", "jumps", "over", "a", "lazy", "dog" };
  auto tree1 = koinos::crypto::merkle_tree< std::string >::create( values1 );
  std::cout << hex_string( tree1.root()->hash() ) << std::endl;

  std::vector< koinos::crypto::digest > values2{ koinos::crypto::hash( "the" ),
                                                 koinos::crypto::hash( "quick" ),
                                                 koinos::crypto::hash( "brown" ),
                                                 koinos::crypto::hash( "fox" ),
                                                 koinos::crypto::hash( "jumps" ),
                                                 koinos::crypto::hash( "over" ),
                                                 koinos::crypto::hash( "a" ),
                                                 koinos::crypto::hash( "lazy" ),
                                                 koinos::crypto::hash( "dog" ) };
  auto tree2 = koinos::crypto::merkle_tree< koinos::crypto::digest, true >::create( values2 );
  std::cout << hex_string( tree2.root()->hash() ) << std::endl;

  std::vector< std::span< const std::byte > > values3;
  for( const auto& value: values2 )
    values3.emplace_back( std::span( value ) );
  auto tree3 = koinos::crypto::merkle_tree< std::span< const std::byte > >::create( values3 );
  std::cout << hex_string( tree3.root()->hash() ) << std::endl;
}
