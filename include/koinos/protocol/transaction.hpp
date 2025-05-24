#pragma once

#include <cstdint>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/operation.hpp>
#include <koinos/protocol/types.hpp>

namespace koinos::protocol {

struct transaction_header
{
  digest network_id;
  uint64_t resource_limit;
  digest operation_merkle_root;
  account payer;
  account payee;
  uint64_t nonce;

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
  account signer;
  account_signature signature;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & signer;
    ar & signature;
  }
};

struct transaction
{
  account id;
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
  digest id;
  bool reverted;
  account payer;
  account payee;
  uint64_t resource_limit;
  uint64_t resource_used;
  uint64_t disk_storage_used;
  uint64_t network_bandwidth_used;
  uint64_t compute_bandwidth_used;
  std::vector< event > events;
  std::vector< log > logs;

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
