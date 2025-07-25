#include <gtest/gtest.h>

#include <respublica/encode/hex.hpp>
#include <respublica/memory/memory.hpp>

using namespace std::string_view_literals;

constexpr std::array< std::uint8_t, 6 > data{ 4, 8, 15, 16, 23, 42 };
constexpr auto valid_hex_str = "0x04080f10172a"sv;

TEST( hex, encode )
{
  auto encoded_data = respublica::encode::to_hex( respublica::memory::as_bytes( data ) );

  EXPECT_EQ( encoded_data, valid_hex_str );
}

TEST( hex, decode )
{
  auto decoded_data = respublica::encode::from_hex( valid_hex_str );

  EXPECT_TRUE( decoded_data );
  EXPECT_TRUE( std::ranges::equal( *decoded_data, respublica::memory::as_bytes( data ) ) );

  decoded_data = respublica::encode::from_hex( valid_hex_str.substr( 2 ) );
  EXPECT_TRUE( decoded_data );
  EXPECT_TRUE( std::ranges::equal( *decoded_data, respublica::memory::as_bytes( data ) ) );

  decoded_data = respublica::encode::from_hex( valid_hex_str.substr( 3 ) );
  if( decoded_data )
    ADD_FAILURE() << "hex decode erroneously succeeded";
  else
  {
    EXPECT_EQ( decoded_data.error().value(), static_cast< int >( respublica::encode::encode_errc::invalid_length ) );
    EXPECT_EQ( decoded_data.error().message(),
               respublica::encode::encode_category().message(
                 static_cast< int >( respublica::encode::encode_errc::invalid_length ) ) );
  }

  decoded_data = respublica::encode::from_hex( "0x0g"sv );
  if( decoded_data )
    ADD_FAILURE() << "hex decode erroneously succeeded";
  else
  {
    EXPECT_EQ( decoded_data.error().value(), static_cast< int >( respublica::encode::encode_errc::invalid_character ) );
    EXPECT_EQ( decoded_data.error().message(),
               respublica::encode::encode_category().message(
                 static_cast< int >( respublica::encode::encode_errc::invalid_character ) ) );
  }
}
