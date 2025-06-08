#include <gtest/gtest.h>
#include <koinos/crypto/hash.hpp>

TEST( hash, blake3 )
{
  auto out = koinos::crypto::hash( "A quick brown fox jumps over the lazy dog" );
  koinos::crypto::digest known{
    std::byte( 0x2a ), std::byte( 0x5f ), std::byte( 0xf5 ), std::byte( 0x14 ), std::byte( 0x5b ), std::byte( 0x9b ),
    std::byte( 0x8e ), std::byte( 0x26 ), std::byte( 0xf8 ), std::byte( 0xeb ), std::byte( 0xe3 ), std::byte( 0xb6 ),
    std::byte( 0x73 ), std::byte( 0x1d ), std::byte( 0x5a ), std::byte( 0x03 ), std::byte( 0x23 ), std::byte( 0x02 ),
    std::byte( 0x9b ), std::byte( 0xe8 ), std::byte( 0xd6 ), std::byte( 0x46 ), std::byte( 0x54 ), std::byte( 0x4c ),
    std::byte( 0x74 ), std::byte( 0xab ), std::byte( 0x6c ), std::byte( 0xea ), std::byte( 0x63 ), std::byte( 0x33 ),
    std::byte( 0x65 ), std::byte( 0x15 ) };
  EXPECT_EQ( out, known );

  std::vector< std::string > out2 = { "A", "quick", "brown", "fox", "jumps", "over", "the", "lazy", "dog" };
  koinos::crypto::hash( out2 );
}
