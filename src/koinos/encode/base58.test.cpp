#include <gtest/gtest.h>

#include <koinos/encode/base58.hpp>

using namespace std::string_literals;

const auto str     = "The quick brown fox jumps over the lazy dog"s;
const auto b58_str = "7DdiPPYtxLjCD3wA1po2rvZHTDYjkZYiEtazrfiwJcwnKCizhGFhBGHeRdx"s;

TEST( base58, encode )
{
  auto encoded_data = koinos::encode::to_base58( std::as_bytes( std::span( str ) ) );

  EXPECT_EQ( encoded_data, b58_str );
}

TEST( base58, decode )
{
  auto decoded_data = koinos::encode::from_base58( b58_str );

  EXPECT_TRUE( std::ranges::equal( decoded_data, std::as_bytes( std::span( str ) ) ) );
}