#include <gtest/gtest.h>
#include <koinos/crypto/hash.hpp>
#include <koinos/util/base58.hpp>

TEST( hash, blake3 )
{
  auto out = koinos::crypto::hash( "A quick brown fox jumps over the lazy dog" );
  EXPECT_EQ( out,
             koinos::util::from_base58< koinos::crypto::digest >( "EzpeBRqqVNxcPGe9caKm9CSHcqnbCftXrc3Efn8R9JmP" ) );

  auto out2 =
    koinos::crypto::hash( std::vector{ "A", "quick", "brown", "fox", "jumps", "over", "the", "lazy", "dog" } );

  EXPECT_EQ( out2,
             koinos::util::from_base58< koinos::crypto::digest >( "4rfJRE9JwBL4vMJMUntTcDF5TRDEmqfU5tfiQFBzt7nV" ) );

  std::vector< std::vector< std::byte > > bytes = {
    {std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 }},
    {std::byte{ 0x04 }, std::byte{ 0x05 }, std::byte{ 0x06 }}
  };
  std::cout << koinos::util::to_base58( koinos::crypto::hash( bytes ) ) << std::endl;

  std::uint64_t number = 12'345;
  std::cout << koinos::util::to_base58( koinos::crypto::hash( number ) ) << std::endl;

  std::vector< std::span< const std::byte > > byte_spans;
  for( const auto& b: bytes )
    byte_spans.push_back( std::span( b ) );

  std::cout << koinos::util::to_base58( koinos::crypto::hash( byte_spans ) ) << std::endl;
}
