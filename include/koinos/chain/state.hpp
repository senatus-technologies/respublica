#pragma once

#include <cstdint>

#include <koinos/crypto/crypto.hpp>
#include <koinos/protocol/protocol.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/util/conversion.hpp>

namespace koinos::chain { namespace state {

namespace zone {

const auto kernel = std::string{};

} // namespace zone

namespace space {

const state_db::object_space& program_bytecode();
const state_db::object_space& program_metadata();
const state_db::object_space& metadata();
const state_db::object_space& transaction_nonce();

} // namespace space

namespace key {

std::span< const std::byte > head_block();
std::span< const std::byte > genesis_key();

} // namespace key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
constexpr uint32_t max_object_size = 512;

} // namespace system_call_dispatch

constexpr uint32_t max_object_size = 1'024 * 1'024; // 1 MB

struct genesis_entry
{
  state_db::object_space space;
  std::vector< std::byte > key;
  std::vector< std::byte > value;
};

using genesis_data = std::vector< genesis_entry >;

struct head
{
  crypto::digest id{};
  uint64_t height = 0;
  crypto::digest previous{};
  uint64_t last_irreversible_block = 0;
  crypto::digest state_merkle_root{};
  uint64_t time = 0;
};

struct resource_limits
{
  uint64_t disk_storage_limit      = 0;
  uint64_t disk_storage_cost       = 0;
  uint64_t network_bandwidth_limit = 0;
  uint64_t network_bandwidth_cost  = 0;
  uint64_t compute_bandwidth_limit = 0;
  uint64_t compute_bandwidth_cost  = 0;
};

}} // namespace koinos::chain::state
