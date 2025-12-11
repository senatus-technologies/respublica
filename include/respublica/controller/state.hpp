#pragma once

#include <cstdint>

#include <respublica/crypto.hpp>
#include <respublica/protocol.hpp>
#include <respublica/state_db.hpp>

namespace respublica::controller { namespace state {

namespace space {

const state_db::object_space& program_data();
const state_db::object_space& metadata();
const state_db::object_space& transaction_nonce();

} // namespace space

namespace key {

std::span< const std::byte > genesis_key();

} // namespace key

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
  std::uint64_t height                  = 0;
  std::uint64_t last_irreversible_block = 0;
  crypto::digest state_merkle_root{};
  std::uint64_t time = 0;
};

struct resource_limits
{
  std::uint64_t disk_storage_limit      = 0;
  std::uint64_t disk_storage_cost       = 0;
  std::uint64_t network_bandwidth_limit = 0;
  std::uint64_t network_bandwidth_cost  = 0;
  std::uint64_t compute_bandwidth_limit = 0;
  std::uint64_t compute_bandwidth_cost  = 0;
};

}} // namespace respublica::controller::state
