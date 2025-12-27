#include <gtest/gtest.h>
#include <respublica/encode.hpp>
#include <respublica/net/peer.hpp>
#include <respublica/net/session.hpp>

using namespace respublica::net;

TEST( peer, id )
{
  // Test peer_id basic properties
  peer_id id1{};
  EXPECT_EQ( id1.size(), peer_id_length );
  EXPECT_EQ( id1.size(), 16 );

  // Test that different peer_ids are different
  peer_id id2;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    id2[ i ] = std::byte{ static_cast< unsigned char >( i ) };
  }

  EXPECT_NE( id1, id2 );
}

TEST( peer, id_to_string )
{
  // Test peer_id_to_string with known value
  peer_id id{};
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    id[ i ] = std::byte{ static_cast< unsigned char >( i ) };
  }

  std::string id_str = peer_id_to_string( id );
  EXPECT_FALSE( id_str.empty() );
  EXPECT_GT( id_str.length(), 0 );

  // Test with all zeros
  peer_id zero_id{};
  std::string zero_str = peer_id_to_string( zero_id );
  EXPECT_FALSE( zero_str.empty() );

  // Different IDs should produce different strings
  EXPECT_NE( id_str, zero_str );
}

TEST( peer, id_to_string_round_trip )
{
  // Test that peer_id can be converted to string and back
  peer_id original_id;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    original_id[ i ] = std::byte{ static_cast< unsigned char >( i * 17 ) };
  }

  std::string id_str = peer_id_to_string( original_id );
  auto decoded       = respublica::encode::from_base58( id_str );

  ASSERT_TRUE( decoded.has_value() );
  EXPECT_EQ( decoded->size(), peer_id_length );
  EXPECT_TRUE( std::ranges::equal( *decoded, original_id ) );
}

TEST( peer, hash_function )
{
  // Test that the hash function works for peer_id
  peer_id id1;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    id1[ i ] = std::byte{ static_cast< unsigned char >( i ) };
  }

  peer_id id2;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    id2[ i ] = std::byte{ static_cast< unsigned char >( i + 1 ) };
  }

  std::hash< peer_id > hasher;
  std::size_t hash1 = hasher( id1 );
  std::size_t hash2 = hasher( id2 );

  // Different peer_ids should (very likely) produce different hashes
  EXPECT_NE( hash1, hash2 );

  // Same peer_id should produce same hash
  EXPECT_EQ( hash1, hasher( id1 ) );
}

TEST( peer, state_management )
{
  // Create a peer with null session (for testing state only)
  peer_id test_id;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    test_id[ i ] = std::byte{ static_cast< unsigned char >( i ) };
  }

  peer test_peer( nullptr, test_id, peer_state::connecting );

  // Test initial state
  EXPECT_EQ( test_peer.state(), peer_state::connecting );
  EXPECT_EQ( test_peer.id(), test_id );

  // Test state transitions
  test_peer.set_state( peer_state::handshaking );
  EXPECT_EQ( test_peer.state(), peer_state::handshaking );

  test_peer.set_state( peer_state::authenticating );
  EXPECT_EQ( test_peer.state(), peer_state::authenticating );

  test_peer.set_state( peer_state::ready );
  EXPECT_EQ( test_peer.state(), peer_state::ready );

  test_peer.set_state( peer_state::reconnecting );
  EXPECT_EQ( test_peer.state(), peer_state::reconnecting );

  test_peer.set_state( peer_state::disconnected );
  EXPECT_EQ( test_peer.state(), peer_state::disconnected );

  test_peer.set_state( peer_state::failed );
  EXPECT_EQ( test_peer.state(), peer_state::failed );
}

TEST( peer, set_id )
{
  // Test that peer ID can be changed
  peer_id initial_id;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    initial_id[ i ] = std::byte{ 0x01 };
  }

  peer_id new_id;
  for( std::size_t i = 0; i < peer_id_length; ++i )
  {
    new_id[ i ] = std::byte{ 0x02 };
  }

  peer test_peer( nullptr, initial_id, peer_state::connecting );
  EXPECT_EQ( test_peer.id(), initial_id );

  test_peer.set_id( new_id );
  EXPECT_EQ( test_peer.id(), new_id );
}

TEST( peer, error_score )
{
  peer_id test_id{};
  peer test_peer( nullptr, test_id, peer_state::ready );

  // Test initial error score
  EXPECT_EQ( test_peer.error_score(), 0 );

  // Test increment
  test_peer.increment_error_score();
  EXPECT_EQ( test_peer.error_score(), 1 );

  test_peer.increment_error_score( 5 );
  EXPECT_EQ( test_peer.error_score(), 6 );

  // Test decrement
  test_peer.decrement_error_score( 2 );
  EXPECT_EQ( test_peer.error_score(), 4 );

  test_peer.decrement_error_score();
  EXPECT_EQ( test_peer.error_score(), 3 );

  // Test decrement below zero (should clamp to 0)
  test_peer.decrement_error_score( 10 );
  EXPECT_EQ( test_peer.error_score(), 0 );

  // Test reset
  test_peer.increment_error_score( 50 );
  EXPECT_EQ( test_peer.error_score(), 50 );

  test_peer.reset_error_score();
  EXPECT_EQ( test_peer.error_score(), 0 );
}

TEST( peer, should_disconnect )
{
  peer_id test_id{};
  peer test_peer( nullptr, test_id, peer_state::ready );

  // Should not disconnect with low error score
  EXPECT_FALSE( test_peer.should_disconnect() );

  // Increment to just below threshold
  test_peer.increment_error_score( default_peer_disconnect_threshold - 1 );
  EXPECT_FALSE( test_peer.should_disconnect() );

  // Increment to threshold
  test_peer.increment_error_score();
  EXPECT_TRUE( test_peer.should_disconnect() );

  // Test with custom threshold
  test_peer.reset_error_score();
  test_peer.increment_error_score( 10 );
  EXPECT_FALSE( test_peer.should_disconnect( 20 ) );
  EXPECT_TRUE( test_peer.should_disconnect( 10 ) );
  EXPECT_TRUE( test_peer.should_disconnect( 5 ) );
}

TEST( peer, session_accessor )
{
  peer_id test_id{};
  peer test_peer( nullptr, test_id, peer_state::connecting );

  // Test that session() returns null when initialized with nullptr
  EXPECT_EQ( test_peer.session(), nullptr );
}
