#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <boost/serialization/string.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/vector.hpp>

#include <respublica/crypto.hpp>
#include <respublica/net/message.hpp>
#include <respublica/protocol/block.hpp>
#include <respublica/protocol/transaction.hpp>

namespace respublica::net {

// Forward declarations
struct handshake_message;
struct ping_message;
struct pong_message;
struct get_blocks_message;
struct block_message;
struct get_peers_message;
struct peers_message;

// Message type traits specializations
template<>
struct message_type_traits< handshake_message >
{
  static constexpr message_type_id type_id = message_type_id::handshake;
};

template<>
struct message_type_traits< ping_message >
{
  static constexpr message_type_id type_id = message_type_id::ping;
};

template<>
struct message_type_traits< pong_message >
{
  static constexpr message_type_id type_id = message_type_id::pong;
};

template<>
struct message_type_traits< get_blocks_message >
{
  static constexpr message_type_id type_id = message_type_id::get_blocks;
};

template<>
struct message_type_traits< block_message >
{
  static constexpr message_type_id type_id = message_type_id::block;
};

template<>
struct message_type_traits< get_peers_message >
{
  static constexpr message_type_id type_id = message_type_id::get_peers;
};

template<>
struct message_type_traits< peers_message >
{
  static constexpr message_type_id type_id = message_type_id::peers;
};

// Handshake message (exchange network ID, protocol version, peer info)
struct handshake_message
{
  crypto::digest network_id{};
  std::uint16_t protocol_version{ current_protocol_version };
  std::string client_version;
  std::uint64_t chain_height{ 0 };

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & network_id;
    ar & protocol_version;
    ar & client_version;
    ar & chain_height;
  }
};

// Ping/Pong for keepalive
struct ping_message
{
  std::uint64_t timestamp{ 0 };
  std::uint64_t nonce{ 0 };

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & timestamp;
    ar & nonce;
  }
};

struct pong_message
{
  std::uint64_t timestamp{ 0 };
  std::uint64_t nonce{ 0 };

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & timestamp;
    ar & nonce;
  }
};

// Request blocks by height range
struct get_blocks_message
{
  std::uint64_t start_height{ 0 };
  std::uint64_t end_height{ 0 };

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & start_height;
    ar & end_height;
  }
};

// Block announcement and response
struct block_message
{
  protocol::block block;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & block;
  }
};

// Peer exchange
struct get_peers_message
{
  template< class Archive >
  void serialize( Archive& /*ar*/, const unsigned int /*version*/ )
  {
    // Empty message
  }
};

struct peers_message
{
  std::vector< std::string > peer_addresses; // Format: "ip:port"

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & peer_addresses;
  }
};

// Union of all network messages
using network_message = std::variant< handshake_message,
                                      ping_message,
                                      pong_message,
                                      get_blocks_message,
                                      block_message,
                                      get_peers_message,
                                      peers_message >;

} // namespace respublica::net
