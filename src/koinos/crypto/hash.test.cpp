// NOLINTBEGIN

#include <gtest/gtest.h>
#include <koinos/crypto/hash.hpp>
#include <koinos/util/base58.hpp>

TEST( hash, blake3 )
{
  auto out = koinos::crypto::hash( "A quick brown fox jumps over the lazy dog" );
  EXPECT_TRUE( std::ranges::equal( out, koinos::util::from_base58( "EzpeBRqqVNxcPGe9caKm9CSHcqnbCftXrc3Efn8R9JmP" ) ) );

  auto out2 =
    koinos::crypto::hash( std::vector{ "A", "quick", "brown", "fox", "jumps", "over", "the", "lazy", "dog" } );

  EXPECT_TRUE( std::ranges::equal( out2, koinos::util::from_base58( "4rfJRE9JwBL4vMJMUntTcDF5TRDEmqfU5tfiQFBzt7nV" ) ) );

  std::vector< std::vector< std::byte > > bytes = {
    {std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 }},
    {std::byte{ 0x04 }, std::byte{ 0x05 }, std::byte{ 0x06 }}
  };
  auto out3 = koinos::crypto::hash( bytes );
  EXPECT_TRUE( std::ranges::equal( out3, koinos::util::from_base58( "9naWkD2RN6dS65KiTDogGSPcrDJaBywdEdD1zwrhHxKs" ) ) );

  std::uint64_t number = 12'345;
  auto out4            = koinos::crypto::hash( number );
  EXPECT_TRUE( std::ranges::equal( out4, koinos::util::from_base58( "HnXg8eU4ELYHMkXCtDRM1paFQnMAGgxrhehAkEgopFh3" ) ) );

  std::vector< std::span< const std::byte > > byte_spans;
  for( const auto& b: bytes )
    byte_spans.push_back( std::span( b ) );

  auto out5 = koinos::crypto::hash( byte_spans );
  EXPECT_TRUE( std::ranges::equal( out5, koinos::util::from_base58( "9naWkD2RN6dS65KiTDogGSPcrDJaBywdEdD1zwrhHxKs" ) ) );
}

// NOLINTEND
