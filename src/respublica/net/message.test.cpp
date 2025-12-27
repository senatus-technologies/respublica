#include <gtest/gtest.h>
#include <respublica/net/message.hpp>
#include <respublica/net/protocol.hpp>

using namespace respublica::net;

TEST( message, frame )
{
  // Test framing a normal message
  std::vector< std::byte > payload{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 }, std::byte{ 0x04 } };
  auto result = frame_message( message_type_id::ping, 1, std::move( payload ) );

  ASSERT_TRUE( result.has_value() );
  EXPECT_EQ( result->header.size(), message_header_size );
  EXPECT_EQ( result->payload.size(), 4 );

  // Verify header contents by parsing it back
  auto header_result = parse_header( std::span< const std::byte >( result->header ) );
  ASSERT_TRUE( header_result.has_value() );
  EXPECT_EQ( header_result->type_id, message_type_id::ping );
  EXPECT_EQ( header_result->version, 1 );
  EXPECT_EQ( header_result->length, sizeof( std::uint32_t ) + sizeof( std::uint16_t ) + 4 );
}

TEST( message, frame_empty_payload )
{
  // Test framing with empty payload
  std::vector< std::byte > payload;
  auto result = frame_message( message_type_id::get_peers, current_protocol_version, std::move( payload ) );

  ASSERT_TRUE( result.has_value() );
  EXPECT_EQ( result->header.size(), message_header_size );
  EXPECT_EQ( result->payload.size(), 0 );

  auto header_result = parse_header( std::span< const std::byte >( result->header ) );
  ASSERT_TRUE( header_result.has_value() );
  EXPECT_EQ( header_result->type_id, message_type_id::get_peers );
  EXPECT_EQ( header_result->length, sizeof( std::uint32_t ) + sizeof( std::uint16_t ) );
}

TEST( message, frame_large_payload )
{
  // Test framing with large payload (1 MB)
  std::vector< std::byte > payload( 1'000'000, std::byte{ 0xAA } );
  auto result = frame_message( message_type_id::block, current_protocol_version, std::move( payload ) );

  ASSERT_TRUE( result.has_value() );
  EXPECT_EQ( result->payload.size(), 1'000'000 );

  auto header_result = parse_header( std::span< const std::byte >( result->header ) );
  ASSERT_TRUE( header_result.has_value() );
  EXPECT_EQ( header_result->length, sizeof( std::uint32_t ) + sizeof( std::uint16_t ) + 1'000'000 );
}

TEST( message, frame_message_too_large )
{
  // Test framing with payload exceeding max size
  std::vector< std::byte > payload( default_max_message_size + 1, std::byte{ 0xFF } );
  auto result = frame_message( message_type_id::block, current_protocol_version, std::move( payload ) );

  ASSERT_FALSE( result.has_value() );
  EXPECT_EQ( result.error(), net_errc::message_too_large );
}

TEST( message, parse_header_valid )
{
  // Create a known header manually
  std::array< std::byte, message_header_size > header_bytes;

  // Length: 4 + 2 + 100 = 106 (little-endian)
  std::uint32_t length = 106;
  std::memcpy( header_bytes.data(), &length, sizeof( std::uint32_t ) );

  // Type ID: ping = 2 (little-endian)
  std::uint32_t type_id = static_cast< std::uint32_t >( message_type_id::ping );
  std::memcpy( header_bytes.data() + 4, &type_id, sizeof( std::uint32_t ) );

  // Version: 1 (little-endian)
  std::uint16_t version = 1;
  std::memcpy( header_bytes.data() + 8, &version, sizeof( std::uint16_t ) );

  auto result = parse_header( std::span< const std::byte >( header_bytes ) );

  ASSERT_TRUE( result.has_value() );
  EXPECT_EQ( result->length, 106 );
  EXPECT_EQ( result->type_id, message_type_id::ping );
  EXPECT_EQ( result->version, 1 );
}

TEST( message, parse_header_incomplete )
{
  // Test with incomplete header (less than 10 bytes)
  std::array< std::byte, 5 > incomplete_header{ std::byte{ 0 } };
  auto result = parse_header( std::span< const std::byte >( incomplete_header ) );

  ASSERT_FALSE( result.has_value() );
  EXPECT_EQ( result.error(), net_errc::incomplete_message );
}

TEST( message, parse_header_message_too_large )
{
  // Create header with length exceeding max size
  std::array< std::byte, message_header_size > header_bytes;

  std::uint32_t length = default_max_message_size + 1'000;
  std::memcpy( header_bytes.data(), &length, sizeof( std::uint32_t ) );

  std::uint32_t type_id = static_cast< std::uint32_t >( message_type_id::handshake );
  std::memcpy( header_bytes.data() + 4, &type_id, sizeof( std::uint32_t ) );

  std::uint16_t version = 1;
  std::memcpy( header_bytes.data() + 8, &version, sizeof( std::uint16_t ) );

  auto result = parse_header( std::span< const std::byte >( header_bytes ) );

  ASSERT_FALSE( result.has_value() );
  EXPECT_EQ( result.error(), net_errc::message_too_large );
}

TEST( message, serialize_deserialize_round_trip )
{
  // Test serialization and deserialization round-trip
  ping_message original_msg;
  original_msg.timestamp = 1'234'567'890;
  original_msg.nonce     = 9'876'543'210;

  auto serialized = serialize_message( original_msg );
  ASSERT_TRUE( serialized.has_value() );
  EXPECT_GT( serialized->size(), 0 );

  auto deserialized = deserialize_message< ping_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->timestamp, original_msg.timestamp );
  EXPECT_EQ( deserialized->nonce, original_msg.nonce );
}

TEST( message, message_type_traits )
{
  // Verify message type traits are correctly defined
  EXPECT_EQ( get_message_type_id< handshake_message >(), message_type_id::handshake );
  EXPECT_EQ( get_message_type_id< ping_message >(), message_type_id::ping );
  EXPECT_EQ( get_message_type_id< pong_message >(), message_type_id::pong );
  EXPECT_EQ( get_message_type_id< block_message >(), message_type_id::block );
  EXPECT_EQ( get_message_type_id< get_peers_message >(), message_type_id::get_peers );
  EXPECT_EQ( get_message_type_id< peers_message >(), message_type_id::peers );
}

TEST( message, frame_all_message_types )
{
  // Test that all message types can be framed
  std::vector< std::byte > payload{ std::byte{ 0x01 } };

  auto result1 = frame_message( message_type_id::handshake, 1, std::vector( payload ) );
  EXPECT_TRUE( result1.has_value() );

  auto result2 = frame_message( message_type_id::ping, 1, std::vector( payload ) );
  EXPECT_TRUE( result2.has_value() );

  auto result3 = frame_message( message_type_id::pong, 1, std::vector( payload ) );
  EXPECT_TRUE( result3.has_value() );

  auto result4 = frame_message( message_type_id::get_blocks, 1, std::vector( payload ) );
  EXPECT_TRUE( result4.has_value() );

  auto result5 = frame_message( message_type_id::block, 1, std::vector( payload ) );
  EXPECT_TRUE( result5.has_value() );

  auto result6 = frame_message( message_type_id::get_peers, 1, std::vector( payload ) );
  EXPECT_TRUE( result6.has_value() );

  auto result7 = frame_message( message_type_id::peers, 1, std::vector( payload ) );
  EXPECT_TRUE( result7.has_value() );
}
