#pragma once

#include <cstdint>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto/crypto.hpp>
#include <koinos/protocol/account.hpp>
#include <koinos/protocol/event.hpp>
#include <koinos/protocol/transaction.hpp>

namespace koinos::protocol {

struct block
{
  crypto::digest id{};
  crypto::digest previous{};
  uint64_t height    = 0;
  uint64_t timestamp = 0;
  crypto::digest state_merkle_root{};
  std::vector< transaction > transactions;
  account signer{};
  crypto::signature signature{};

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & previous;
    ar & height;
    ar & timestamp;
    ar & state_merkle_root;
    ar & transactions;
    ar & signer;
    ar & signature;
  }

  std::size_t size() const noexcept;
  bool validate() const noexcept;
};

struct block_receipt
{
  crypto::digest id{};
  uint64_t height                 = 0;
  uint64_t disk_storage_used      = 0;
  uint64_t network_bandwidth_used = 0;
  uint64_t compute_bandwidth_used = 0;
  crypto::digest state_merkle_root{};
  std::vector< event > events;
  std::vector< transaction_receipt > transaction_receipts;
  std::vector< std::string > logs;
  uint64_t disk_storage_charged      = 0;
  uint64_t network_bandwidth_charged = 0;
  uint64_t compute_bandwidth_charged = 0;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & height;
    ar & disk_storage_used;
    ar & network_bandwidth_used;
    ar & compute_bandwidth_used;
    ar & state_merkle_root;
    ar & events;
    ar & transaction_receipts;
    ar & logs;
    ar & disk_storage_charged;
    ar & network_bandwidth_charged;
    ar & compute_bandwidth_charged;
  }
};

crypto::digest make_id( const block& b ) noexcept;

} // namespace koinos::protocol
