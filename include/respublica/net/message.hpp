#pragma once

#include <cstdint>
#include <span>
#include <sstream>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <respublica/net/error.hpp>

namespace respublica::net {

// Stable message type IDs (explicit enum for cross-platform/cross-compiler compatibility)
enum class message_type_id : std::uint32_t // NOLINT(performance-enum-size)
{
  handshake  = 1,
  ping       = 2,
  pong       = 3,
  get_blocks = 4,
  block      = 5,
  get_peers  = 6,
  peers      = 7,
  // Reserve range 1-999 for core protocol messages
  // Range 1000+ available for custom application messages
};

// Protocol version
constexpr std::uint16_t current_protocol_version = 1;

// Maximum message size (10 MB default)
constexpr std::size_t default_max_message_size = 10'485'760;

// Message header (10 bytes)
struct message_header
{
  std::uint32_t length{ 0 };                             // Total size (type + version + payload)
  message_type_id type_id{ message_type_id::handshake }; // Message type discriminator
  std::uint16_t version{ current_protocol_version };
};

constexpr std::size_t message_header_size =
  sizeof( std::uint32_t ) + sizeof( message_type_id ) + sizeof( std::uint16_t );

// Scatter-gather message frame (avoids copying payload)
struct message_frame
{
  // Header bytes (10 bytes: length + type_id + version)
  std::array< std::byte, message_header_size > header;
  // Payload (owned)
  std::vector< std::byte > payload;
};

// Type ID mapping trait (must be specialized for each message type)
template< typename T >
struct message_type_traits;

// Helper to get message type ID from type
template< typename T >
constexpr message_type_id get_message_type_id() noexcept
{
  return message_type_traits< T >::type_id;
}

// Serialize message to bytes
template< typename T >
result< std::vector< std::byte > > serialize_message( const T& message )
{
  try
  {
    std::ostringstream oss( std::ios::binary );
    boost::archive::binary_oarchive oa( oss );
    oa << message;

    const std::string& str = oss.str();
    std::vector< std::byte > payload( str.size() );
    std::memcpy( payload.data(), str.data(), str.size() );

    return payload;
  }
  catch( const std::exception& )
  {
    return std::unexpected( net_errc::serialization_failed );
  }
}

// Deserialize message from bytes
template< typename T >
result< T > deserialize_message( std::span< const std::byte > data )
{
  try
  {
    std::string str( data.size(), '\0' );
    std::memcpy( str.data(), data.data(), data.size() );

    std::istringstream iss( str, std::ios::binary );
    boost::archive::binary_iarchive ia( iss );

    T message;
    ia >> message;

    return message;
  }
  catch( const std::exception& )
  {
    return std::unexpected( net_errc::deserialization_failed );
  }
}

// Frame message (zero-copy for payload via scatter-gather I/O)
result< message_frame >
frame_message( message_type_id type_id, std::uint16_t version, std::vector< std::byte > payload );

// Parse message header from bytes
result< message_header > parse_header( std::span< const std::byte > data );

} // namespace respublica::net
