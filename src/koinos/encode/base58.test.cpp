#include <gtest/gtest.h>

#include <koinos/encode/base58.hpp>

using namespace std::string_view_literals;

const auto str     = "The quick brown fox jumps over the lazy dog"sv;
const auto b58_str = "7DdiPPYtxLjCD3wA1po2rvZHTDYjkZYiEtazrfiwJcwnKCizhGFhBGHeRdx"sv;

TEST( base58, encode )
{
  auto encoded_data = koinos::encode::to_base58( std::as_bytes( std::span( str ) ) );

  EXPECT_EQ( encoded_data, b58_str );
}

TEST( base58, decode )
{
  auto decoded_data = koinos::encode::from_base58( b58_str );

  EXPECT_TRUE( decoded_data );
  EXPECT_TRUE( std::ranges::equal( *decoded_data, std::as_bytes( std::span( str ) ) ) );

  decoded_data = koinos::encode::from_base58( "0DdiPPYtxLjCD3wA1po2rvZHTDYjkZYiEtazrfiwJcwnKCizhGFhBGHeRdx"sv );
  if( decoded_data )
    ADD_FAILURE() << "base58 decode erroneously succeeded";
  else
  {
    EXPECT_EQ( decoded_data.error().value(), static_cast< int >( koinos::encode::encode_errc::invalid_character ) );
    EXPECT_EQ( decoded_data.error().message(),
               koinos::encode::encode_category{}.message(
                 static_cast< int >( koinos::encode::encode_errc::invalid_character ) ) );
  }
}
