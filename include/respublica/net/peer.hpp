#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace respublica::net {

class session;

constexpr std::size_t peer_id_length                      = 16;
constexpr std::uint32_t default_peer_disconnect_threshold = 100;

// UUID type (128-bit identifier)
using peer_id = std::array< std::byte, peer_id_length >;

// Convert peer_id to hex string for display
std::string peer_id_to_string( const peer_id& id );

// Generate peer_id from private key using BLAKE3
peer_id generate_peer_id( const std::filesystem::path& private_key_path );

class peer
{
public:
  peer( std::shared_ptr< net::session > sess, peer_id id );

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
  std::uint32_t _error_score{ 0 };
};

} // namespace respublica::net
