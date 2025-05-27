#pragma once

#include <cstdint>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/operation.hpp>
#include <koinos/protocol/types.hpp>

namespace koinos::protocol {

struct transaction_header
{
  digest network_id{ std::byte{ 0x00 } };
  uint64_t resource_limit = 0;
  digest operation_merkle_root{ std::byte{ 0x00 } };
  account payer{ std::byte{ 0x00 } };
  account payee{ std::byte{ 0x00 } };
  uint64_t nonce = 0;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & network_id;
    ar & resource_limit;
    ar & operation_merkle_root;
    ar & payer;
    ar & payee;
    ar & nonce;
  }
};

struct transaction_signature
{
  account signer{ std::byte{ 0x00 } };
  account_signature signature{ std::byte{ 0x00 } };

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & signer;
    ar & signature;
  }
};

struct transaction
{
  account id{ std::byte{ 0x00 } };
  transaction_header header;
  std::vector< operation > operations;
  std::vector< transaction_signature > signatures;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & header;
    ar & operations;
    ar & signatures;
  }
};

struct transaction_receipt
{
  digest id{ std::byte{ 0x00 } };
  bool reverted = false;
  account payer{ std::byte{ 0x00 } };
  account payee{ std::byte{ 0x00 } };
  uint64_t resource_limit         = 0;
  uint64_t resource_used          = 0;
  uint64_t disk_storage_used      = 0;
  uint64_t network_bandwidth_used = 0;
  uint64_t compute_bandwidth_used = 0;
  std::vector< event > events;
  std::vector< std::string > logs;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & reverted;
    ar & payer;
    ar & payee;
    ar & resource_limit;
    ar & resource_used;
    ar & disk_storage_used;
    ar & network_bandwidth_used;
    ar & compute_bandwidth_used;
    ar & events;
    ar & logs;
  }
};

} // namespace koinos::protocol

template< typename T >
concept Transaction = std::same_as< koinos::protocol::transaction, T >;
