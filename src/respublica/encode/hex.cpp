#include <respublica/encode/hex.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace respublica::encode {

constexpr char hex_offset = 10;

std::string to_hex( std::span< const std::byte > s ) noexcept
{
  std::stringstream stream;
  stream << "0x" << std::hex << std::setfill( '0' );
  for( const auto& b: s )
    stream << std::setw( 2 ) << static_cast< unsigned int >( std::bit_cast< unsigned char >( b ) );

  return stream.str();
}

result< std::uint8_t > hex_to_char( char in ) noexcept
{
  if( in >= '0' && in <= '9' )
    return in - '0';
  if( in >= 'a' && in <= 'f' )
    return in - 'a' + hex_offset;
  if( in >= 'A' && in <= 'F' )
    return in - 'A' + hex_offset;

  return std::unexpected( encode_errc::invalid_character );
}

result< std::vector< std::byte > > from_hex( std::string_view sv ) noexcept
{
  if( sv.size() % 2 != 0 )
    return std::unexpected( encode_errc::invalid_length );

  std::vector< std::byte > bytes;
  bytes.reserve( sv.size() / 2 );

  for( std::size_t i = sv.at( 0 ) == '0' && sv.at( 1 ) == 'x' ? 2 : 0; i < sv.size(); i += 2 )
  {
    if( auto first_char = hex_to_char( sv.at( i ) ); first_char )
    {
      if( auto second_char = hex_to_char( sv.at( i + 1 ) ); second_char )
        bytes.push_back( static_cast< std::byte >( *first_char << 4 | *second_char ) );
      else
        return std::unexpected( second_char.error() );
    }
    else
      return std::unexpected( first_char.error() );
  }

  return bytes;
}

} // namespace respublica::encode
