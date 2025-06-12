#include <gtest/gtest.h>

#include <koinos/encode/hex.hpp>

using namespace std::string_literals;

constexpr uint8_t data[] = { 4, 8, 15, 16, 23, 42 };
constexpr auto hex_str = "0x04080f10172a"s;

TEST( hex, encode )
{
  auto encoded_data = koinos::encode::to_hex( std::as_bytes( std::span( data ) ) );

  EXPECT_EQ( encoded_data, hex_str );
}

TEST( hex, decode )
{
  auto decoded_data = koinos::encode::from_hex( hex_str );

  EXPECT_TRUE( std::ranges::equal( decoded_data, std::as_bytes( std::span( data ) ) ) );
}