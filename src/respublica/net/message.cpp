#include <respublica/net/message.hpp>

#include <bit>
#include <cstring>

namespace respublica::net {

result< std::vector< std::byte > >
frame_message( message_type_id type_id, std::uint16_t version, std::span< const std::byte > payload )
{
  if( payload.size() > default_max_message_size )
  {
    return std::unexpected( net_errc::message_too_large );
  }

  const std::uint32_t total_length =
    static_cast< std::uint32_t >( sizeof( std::uint32_t ) + sizeof( std::uint16_t ) + payload.size() );

  std::vector< std::byte > frame( message_header_size + payload.size() );
  std::size_t offset = 0;

  // Write length (little-endian)
  std::uint32_t length_le = total_length;
  if constexpr( std::endian::native == std::endian::big )
    length_le = std::byteswap( length_le );
  std::memcpy( frame.data() + offset, &length_le, sizeof( std::uint32_t ) );
  offset += sizeof( std::uint32_t );

  // Write type ID (little-endian)
  std::uint32_t type_id_le = static_cast< std::uint32_t >( type_id );
  if constexpr( std::endian::native == std::endian::big )
    type_id_le = std::byteswap( type_id_le );
  std::memcpy( frame.data() + offset, &type_id_le, sizeof( std::uint32_t ) );
  offset += sizeof( std::uint32_t );

  // Write version (little-endian)
  std::uint16_t version_le = version;
  if constexpr( std::endian::native == std::endian::big )
    version_le = std::byteswap( version_le );
  std::memcpy( frame.data() + offset, &version_le, sizeof( std::uint16_t ) );
  offset += sizeof( std::uint16_t );

  // Write payload
  std::memcpy( frame.data() + offset, payload.data(), payload.size() );

  return frame;
}

result< message_header > parse_header( std::span< const std::byte > data )
{
  if( data.size() < message_header_size )
  {
    return std::unexpected( net_errc::incomplete_message );
  }

  message_header header;
  std::size_t offset = 0;

  // Read length
  std::memcpy( &header.length, data.data() + offset, sizeof( std::uint32_t ) );
  if constexpr( std::endian::native == std::endian::big )
    header.length = std::byteswap( header.length );
  offset += sizeof( std::uint32_t );

  // Read type ID
  std::uint32_t type_id_raw = 0;
  std::memcpy( &type_id_raw, data.data() + offset, sizeof( std::uint32_t ) );
  if constexpr( std::endian::native == std::endian::big )
    type_id_raw = std::byteswap( type_id_raw );
  header.type_id  = static_cast< message_type_id >( type_id_raw );
  offset         += sizeof( std::uint32_t );

  // Read version
  std::memcpy( &header.version, data.data() + offset, sizeof( std::uint16_t ) );
  if constexpr( std::endian::native == std::endian::big )
    header.version = std::byteswap( header.version );

  // Validate
  if( header.length > default_max_message_size )
  {
    return std::unexpected( net_errc::message_too_large );
  }

  return header;
}

} // namespace respublica::net
