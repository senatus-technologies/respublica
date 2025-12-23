#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <openssl/x509.h>

namespace respublica::net {

class session;

constexpr std::size_t peer_id_length                      = 16;
constexpr std::uint32_t default_peer_disconnect_threshold = 100;

// UUID type (128-bit identifier)
using peer_id = std::array< std::byte, peer_id_length >;

// Peer state enumeration
enum class peer_state
{
  connecting,     // Initial TCP connection established
  handshaking,    // TLS handshake in progress
  authenticating, // Verifying peer identity/certificate
  ready,          // Handshake complete, can send/receive
  reconnecting,   // Attempting to reconnect after disconnect
  disconnected,   // Permanently disconnected
  failed          // Connection failed, should be removed
};

// Convert peer_id to hex string for display
std::string peer_id_to_string( const peer_id& id );

// Generate peer_id from private key using BLAKE3
peer_id generate_peer_id( const std::filesystem::path& private_key_path );

// Extract peer_id from X509 certificate using BLAKE3
peer_id extract_peer_id_from_certificate( X509* cert );

class peer
{
public:
  peer( std::shared_ptr< net::session > sess, peer_id id, peer_state initial_state = peer_state::connecting );

  // Get the underlying session
  std::shared_ptr< net::session > session() const
  {
    return _session;
  }

  // Get peer ID
  const peer_id& id() const
  {
    return _id;
  }

  // Set peer ID (used after handshake to set actual remote peer ID)
  void set_id( peer_id id )
  {
    _id = id;
  }

  // Get current state (thread-safe)
  peer_state state() const
  {
    return _state.load( std::memory_order_acquire );
  }

  // Set state (thread-safe)
  void set_state( peer_state new_state )
  {
    _state.store( new_state, std::memory_order_release );
  }

  // Get error score
  std::uint32_t error_score() const
  {
    return _error_score;
  }

  // Increment error score (e.g., for protocol violations, timeouts, etc.)
  void increment_error_score( std::uint32_t amount = 1 )
  {
    _error_score += amount;
  }

  // Decrement error score (e.g., for successful interactions)
  void decrement_error_score( std::uint32_t amount = 1 )
  {
    if( _error_score >= amount )
      _error_score -= amount;
    else
      _error_score = 0;
  }

  // Reset error score
  void reset_error_score()
  {
    _error_score = 0;
  }

  // Check if peer should be disconnected based on error threshold
  bool should_disconnect( std::uint32_t threshold = default_peer_disconnect_threshold ) const
  {
    return _error_score >= threshold;
  }

private:
  std::shared_ptr< net::session > _session;
  peer_id _id;
  std::atomic< peer_state > _state;
  std::uint32_t _error_score{ 0 };
};

} // namespace respublica::net
