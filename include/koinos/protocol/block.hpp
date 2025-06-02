#pragma once

#include <cstdint>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/transaction.hpp>
#include <koinos/protocol/types.hpp>

namespace koinos::protocol {

struct block_header
{
  digest previous{};
  uint64_t height    = 0;
  uint64_t timestamp = 0;
  digest previous_state_merkle_root{};
  digest transaction_merkle_root{};
  account signer{};

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & previous;
    ar & height;
    ar & timestamp;
    ar & previous_state_merkle_root;
    ar & transaction_merkle_root;
    ar & signer;
  }
};

struct block
{
  digest id{};
  block_header header;
  std::vector< transaction > transactions;
  account_signature signature{};

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & header;
    ar & transactions;
    ar & signature;
  }
};

struct block_receipt
{
  digest id{};
  uint64_t height                 = 0;
  uint64_t disk_storage_used      = 0;
  uint64_t network_bandwidth_used = 0;
  uint64_t compute_bandwidth_used = 0;
  digest state_merkle_root{};
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

} // namespace koinos::protocol
