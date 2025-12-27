#include <gtest/gtest.h>
#include <respublica/net/message.hpp>
#include <respublica/net/messages.hpp>

using namespace respublica::net;

TEST( messages, serialization )
{
  // Test handshake_message serialization/deserialization
  handshake_message handshake;
  handshake.network_id       = respublica::crypto::digest{ std::byte{ 0x01 }, std::byte{ 0x02 } };
  handshake.protocol_version = 42;
  handshake.client_version   = "test-client-1.0";
  handshake.chain_height     = 12'345;

  auto serialized = serialize_message( handshake );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< handshake_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_TRUE( std::ranges::equal( deserialized->network_id, handshake.network_id ) );
  EXPECT_EQ( deserialized->protocol_version, handshake.protocol_version );
  EXPECT_EQ( deserialized->client_version, handshake.client_version );
  EXPECT_EQ( deserialized->chain_height, handshake.chain_height );
}

TEST( messages, ping_message_serialization )
{
  ping_message ping;
  ping.timestamp = 1'234'567'890;
  ping.nonce     = 9'876'543'210;

  auto serialized = serialize_message( ping );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< ping_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->timestamp, ping.timestamp );
  EXPECT_EQ( deserialized->nonce, ping.nonce );
}

TEST( messages, pong_message_serialization )
{
  pong_message pong;
  pong.timestamp = 1'111'111'111;
  pong.nonce     = 2'222'222'222;

  auto serialized = serialize_message( pong );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< pong_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->timestamp, pong.timestamp );
  EXPECT_EQ( deserialized->nonce, pong.nonce );
}

TEST( messages, get_blocks_message_serialization )
{
  get_blocks_message get_blocks;
  get_blocks.start_height = 100;
  get_blocks.end_height   = 200;

  auto serialized = serialize_message( get_blocks );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< get_blocks_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->start_height, get_blocks.start_height );
  EXPECT_EQ( deserialized->end_height, get_blocks.end_height );
}

TEST( messages, get_peers_message_serialization )
{
  // get_peers_message is empty, but should still serialize/deserialize
  get_peers_message get_peers;

  auto serialized = serialize_message( get_peers );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< get_peers_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
}

TEST( messages, peers_message_serialization )
{
  peers_message peers;
  peers.peer_addresses = { "192.168.1.1:8080", "10.0.0.1:9000", "example.com:7777" };

  auto serialized = serialize_message( peers );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< peers_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->peer_addresses.size(), 3 );
  EXPECT_EQ( deserialized->peer_addresses[ 0 ], "192.168.1.1:8080" );
  EXPECT_EQ( deserialized->peer_addresses[ 1 ], "10.0.0.1:9000" );
  EXPECT_EQ( deserialized->peer_addresses[ 2 ], "example.com:7777" );
}

TEST( messages, peers_message_empty_list )
{
  // Test with empty peer list
  peers_message peers;
  peers.peer_addresses = {};

  auto serialized = serialize_message( peers );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< peers_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->peer_addresses.size(), 0 );
}

TEST( messages, handshake_default_values )
{
  // Test that default values are sensible
  handshake_message handshake;

  EXPECT_EQ( handshake.protocol_version, current_protocol_version );
  EXPECT_EQ( handshake.chain_height, 0 );
  EXPECT_TRUE( handshake.client_version.empty() );
}

TEST( messages, ping_pong_default_values )
{
  ping_message ping;
  EXPECT_EQ( ping.timestamp, 0 );
  EXPECT_EQ( ping.nonce, 0 );

  pong_message pong;
  EXPECT_EQ( pong.timestamp, 0 );
  EXPECT_EQ( pong.nonce, 0 );
}

TEST( messages, get_blocks_default_values )
{
  get_blocks_message get_blocks;
  EXPECT_EQ( get_blocks.start_height, 0 );
  EXPECT_EQ( get_blocks.end_height, 0 );
}

TEST( messages, handshake_with_long_client_version )
{
  // Test with long client version string
  handshake_message handshake;
  handshake.client_version   = std::string( 1'000, 'A' );
  handshake.protocol_version = 1;
  handshake.chain_height     = 999;

  auto serialized = serialize_message( handshake );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< handshake_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->client_version.length(), 1'000 );
  EXPECT_EQ( deserialized->protocol_version, 1 );
  EXPECT_EQ( deserialized->chain_height, 999 );
}

TEST( messages, peers_message_with_many_peers )
{
  // Test with many peers
  peers_message peers;
  for( int i = 0; i < 100; ++i )
  {
    peers.peer_addresses.push_back( "peer" + std::to_string( i ) + ".example.com:8080" );
  }

  auto serialized = serialize_message( peers );
  ASSERT_TRUE( serialized.has_value() );

  auto deserialized = deserialize_message< peers_message >( *serialized );
  ASSERT_TRUE( deserialized.has_value() );
  EXPECT_EQ( deserialized->peer_addresses.size(), 100 );
  EXPECT_EQ( deserialized->peer_addresses[ 0 ], "peer0.example.com:8080" );
  EXPECT_EQ( deserialized->peer_addresses[ 99 ], "peer99.example.com:8080" );
}

TEST( messages, message_type_id_values )
{
  // Verify message type ID values are as expected (for wire protocol stability)
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::handshake ), 1 );
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::ping ), 2 );
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::pong ), 3 );
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::get_blocks ), 4 );
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::block ), 5 );
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::get_peers ), 6 );
  EXPECT_EQ( static_cast< std::uint32_t >( message_type_id::peers ), 7 );
}

TEST( messages, current_protocol_version )
{
  // Ensure protocol version is defined
  EXPECT_GT( current_protocol_version, 0 );
}

TEST( messages, max_message_size )
{
  // Verify default max message size is reasonable
  EXPECT_EQ( default_max_message_size, 10'485'760 ); // 10 MB
}
