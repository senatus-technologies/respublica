#pragma once

#include <cstdint>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto.hpp>
#include <koinos/protocol/account.hpp>
#include <koinos/protocol/event.hpp>
#include <koinos/protocol/program.hpp>
#include <koinos/protocol/transaction.hpp>

namespace koinos::protocol {

struct block
{
  crypto::digest id{};
  crypto::digest previous{};
  std::uint64_t height    = 0;
  std::uint64_t timestamp = 0;
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
  std::uint64_t height = 0;
  std::vector< program_frame > frames;
  std::uint64_t disk_storage_used      = 0;
  std::uint64_t network_bandwidth_used = 0;
  std::uint64_t compute_bandwidth_used = 0;
  crypto::digest state_merkle_root{};
  std::vector< transaction_receipt > transaction_receipts;
  std::uint64_t disk_storage_charged      = 0;
  std::uint64_t network_bandwidth_charged = 0;
  std::uint64_t compute_bandwidth_charged = 0;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & height;
    ar & frames;
    ar & disk_storage_used;
    ar & network_bandwidth_used;
    ar & compute_bandwidth_used;
    ar & state_merkle_root;
    ar & transaction_receipts;
    ar & disk_storage_charged;
    ar & network_bandwidth_charged;
    ar & compute_bandwidth_charged;
  }
};

crypto::digest make_id( const block& b ) noexcept;

} // namespace koinos::protocol
