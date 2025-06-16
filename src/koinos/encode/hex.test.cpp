#include <gtest/gtest.h>

#include <koinos/encode/hex.hpp>

using namespace std::string_view_literals;

constexpr std::array< std::uint8_t, 6 > data{ 4, 8, 15, 16, 23, 42 };
constexpr auto valid_hex_str = "0x04080f10172a"sv;

TEST( hex, encode )
{
  auto encoded_data = koinos::encode::to_hex( std::as_bytes( std::span( data ) ) );

  EXPECT_EQ( encoded_data, valid_hex_str );
}

TEST( hex, decode )
{
  auto decoded_data = koinos::encode::from_hex( valid_hex_str );

  EXPECT_TRUE( decoded_data );
  EXPECT_TRUE( std::ranges::equal( *decoded_data, std::as_bytes( std::span( data ) ) ) );

  decoded_data = koinos::encode::from_hex( valid_hex_str.substr( 2 ) );
  EXPECT_TRUE( decoded_data );
  EXPECT_TRUE( std::ranges::equal( *decoded_data, std::as_bytes( std::span( data ) ) ) );

  decoded_data = koinos::encode::from_hex( valid_hex_str.substr( 3 ) );
  if( decoded_data )
    ADD_FAILURE() << "hex decode erroneously succeeded";
  else
  {
    EXPECT_EQ( decoded_data.error().value(), static_cast< int >( koinos::encode::encode_errc::invalid_length ) );
    EXPECT_EQ(
      decoded_data.error().message(),
      koinos::encode::encode_category{}.message( static_cast< int >( koinos::encode::encode_errc::invalid_length ) ) );
  }

  decoded_data = koinos::encode::from_hex( "0x0g"sv );
  if( decoded_data )
    ADD_FAILURE() << "hex decode erroneously succeeded";
  else
  {
    EXPECT_EQ( decoded_data.error().value(), static_cast< int >( koinos::encode::encode_errc::invalid_character ) );
    EXPECT_EQ( decoded_data.error().message(),
               koinos::encode::encode_category{}.message(
                 static_cast< int >( koinos::encode::encode_errc::invalid_character ) ) );
  }
}
