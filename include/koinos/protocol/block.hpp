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
  digest previous{ std::byte{ 0x00 } };
  uint64_t height    = 0;
  uint64_t timestamp = 0;
  digest previous_state_merkle_root{ std::byte{ 0x00 } };
  digest transaction_merkle_root{ std::byte{ 0x00 } };
  account signer{ std::byte{ 0x00 } };

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
  digest id{ std::byte{ 0x00 } };
  block_header header;
  std::vector< transaction > transactions;
  account_signature signature{ std::byte{ 0x00 } };

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
  digest id{ std::byte{ 0x00 } };
  uint64_t height                 = 0;
  uint64_t disk_storage_used      = 0;
  uint64_t network_bandwidth_used = 0;
  uint64_t compute_bandwidth_used = 0;
  digest state_merkle_root{ std::byte{ 0x00 } };
  std::vector< event > events;
  std::vector< transaction_receipt > transaction_receipts;
  std::vector< log > logs;
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
