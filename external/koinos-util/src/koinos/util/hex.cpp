#include <koinos/util/hex.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace koinos::util {

std::string to_hex( std::span< const std::byte > s )
{
  std::stringstream stream;
  stream << "0x" << std::hex << std::setfill( '0' );
  for( const auto& b: s )
  {
    stream << std::setw( 2 ) << std::bit_cast< uint8_t >( b );
  }
  return stream.str();
}

char hex_to_char( char in )
{
  if( in >= '0' && in <= '9' )
    return in - '0';
  if( in >= 'a' && in <= 'f' )
    return in - 'a' + 10;
  if( in >= 'A' && in <= 'F' )
    return in - 'A' + 10;
  throw std::logic_error( "input is not hex" );
}

std::vector< std::byte > from_hex( std::string_view sv )
{
  std::vector< std::byte > bytes;
  bytes.reserve( sv.size() / 2 );
  auto data = sv.data();

  for( size_t i = data[ 0 ] == '0' && data[ 1 ] == 'x' ? 2 : 0; i < sv.size(); i += 2 )
    bytes.push_back( std::byte{ uint8_t( hex_to_char( data[ i ] ) << 4 | hex_to_char( data[ i + 1 ] ) ) } );

  return bytes;
}

} // namespace koinos::util
