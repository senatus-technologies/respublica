#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <boost/asio.hpp>
#include <openssl/x509.h>

namespace respublica::net {

class session;

constexpr std::size_t peer_id_length                      = 16;
constexpr std::uint32_t default_peer_disconnect_threshold = 100;

// UUID type (128-bit identifier)
using peer_id = std::array< std::byte, peer_id_length >;

// Peer state enumeration
enum class peer_state : std::uint_fast8_t
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

// Forward declaration
class peer;

// Read-only view of peer - exposes only safe observation methods
class peer_view
{
public:
  peer_view( std::shared_ptr< peer > p ):
      _peer( std::move( p ) )
  {}

  // Identity
  const peer_id& id() const;

  // State observation (thread-safe via atomics)
  peer_state state() const;
  std::uint32_t error_score() const;
  int reconnect_attempts() const;

  // Connection info
  bool has_endpoint() const;

  // Health check
  bool should_disconnect( std::uint32_t threshold = default_peer_disconnect_threshold ) const;

private:
  std::shared_ptr< peer > _peer; // Keep peer alive but hide mutating operations
};

class peer
{
public:
  peer( std::shared_ptr< net::session > sess, peer_id id, peer_state initial_state ):
      _session( std::move( sess ) ),
      _id( id ),
      _state( initial_state )
  {}

  // Get the underlying session (may be null during reconnection)
  std::shared_ptr< net::session > session() const
  {
    return _session;
  }

  // Replace session during reconnection
  void replace_session( std::shared_ptr< net::session > new_session )
  {
    _session = std::move( new_session );
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

  // Get error score (thread-safe)
  std::uint32_t error_score() const
  {
    return _error_score.load( std::memory_order_relaxed );
  }

  // Increment error score (thread-safe, e.g., for protocol violations, timeouts, etc.)
  void increment_error_score( std::uint32_t amount = 1 )
  {
    _error_score.fetch_add( amount, std::memory_order_relaxed );
  }

  // Decrement error score (thread-safe, e.g., for successful interactions)
  void decrement_error_score( std::uint32_t amount = 1 )
  {
    // Use compare-exchange loop to prevent underflow
    std::uint32_t current = _error_score.load( std::memory_order_relaxed );
    std::uint32_t desired;
    do
    {
      desired = ( current >= amount ) ? ( current - amount ) : 0;
    }
    while(
      !_error_score.compare_exchange_weak( current, desired, std::memory_order_relaxed, std::memory_order_relaxed ) );
  }

  // Reset error score (thread-safe)
  void reset_error_score()
  {
    _error_score.store( 0, std::memory_order_relaxed );
  }

  // Check if peer should be disconnected based on error threshold (thread-safe)
  bool should_disconnect( std::uint32_t threshold = default_peer_disconnect_threshold ) const
  {
    return _error_score.load( std::memory_order_relaxed ) >= threshold;
  }

  // Reconnection state management (thread-safe)
  int reconnect_attempts() const
  {
    return _reconnect_attempts.load( std::memory_order_relaxed );
  }

  void increment_reconnect_attempts()
  {
    _reconnect_attempts.fetch_add( 1, std::memory_order_relaxed );
  }

  void reset_reconnect_attempts()
  {
    _reconnect_attempts.store( 0, std::memory_order_relaxed );
  }

  // Endpoint for reconnection
  void set_endpoint( boost::asio::ip::tcp::resolver::results_type endpoint )
  {
    _endpoint = std::move( endpoint );
  }

  const std::optional< boost::asio::ip::tcp::resolver::results_type >& endpoint() const
  {
    return _endpoint;
  }

private:
  std::shared_ptr< net::session > _session;
  peer_id _id;
  std::atomic< peer_state > _state;
  std::atomic< std::uint32_t > _error_score{ 0 };
  std::atomic< int > _reconnect_attempts{ 0 };
  std::optional< boost::asio::ip::tcp::resolver::results_type > _endpoint;
};

} // namespace respublica::net

// Hash specialization for peer_id to enable std::unordered_map usage
namespace std {

template<>
struct hash< respublica::net::peer_id >
{
  std::size_t operator()( const respublica::net::peer_id& id ) const noexcept
  {
    // peer_id is std::array<std::byte, 16> from BLAKE3 hash
    // First 8 bytes are already well-distributed, use them directly as hash
    std::size_t result = 0;
    std::memcpy( &result, id.data(), sizeof( std::size_t ) );
    return result;
  }
};

} // namespace std
