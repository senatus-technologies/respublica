#pragma once

#include <cstdint>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto/crypto.hpp>
#include <koinos/protocol/account.hpp>
#include <koinos/protocol/event.hpp>
#include <koinos/protocol/operation.hpp>

namespace koinos::protocol {

struct transaction
{
  crypto::digest id{};
  crypto::digest network_id{};
  std::uint64_t resource_limit = 0;
  account payer{};
  account payee{};
  std::uint64_t nonce = 0;
  std::vector< operation > operations;
  std::vector< authorization > authorizations;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & network_id;
    ar & resource_limit;
    ar & payer;
    ar & payee;
    ar & nonce;
    ar & operations;
    ar & authorizations;
  }

  std::size_t size() const noexcept;
  bool validate() const noexcept;
};

struct transaction_receipt
{
  crypto::digest id{};
  bool reverted = false;
  account payer{};
  account payee{};
  std::uint64_t resource_limit         = 0;
  std::uint64_t resource_used          = 0;
  std::uint64_t disk_storage_used      = 0;
  std::uint64_t network_bandwidth_used = 0;
  std::uint64_t compute_bandwidth_used = 0;
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

crypto::digest make_id( const transaction& t ) noexcept;

} // namespace koinos::protocol

template< typename T >
concept Transaction = std::same_as< koinos::protocol::transaction, T >;
