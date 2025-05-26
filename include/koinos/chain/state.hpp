#pragma once

#include <cstdint>

#include <koinos/crypto/multihash.hpp>
#include <koinos/protocol/protocol.hpp>
#include <koinos/state_db/state_db_types.hpp>
#include <koinos/util/conversion.hpp>

namespace koinos::chain { namespace state {

namespace zone {

const auto kernel = std::string{};

} // namespace zone

namespace space {

const state_db::object_space program_bytecode();
const state_db::object_space program_metadata();
const state_db::object_space metadata();
const state_db::object_space transaction_nonce();

} // namespace space

namespace key {

const auto head_block = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::head_block" ) ) );
const auto chain_id = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::chain_id" ) ) );
const auto genesis_key = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::genesis_key" ) ) );
const auto resource_limit_data = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::resource_limit_data" ) ) );
const auto max_account_resources = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::max_account_resources" ) ) );
const auto protocol_descriptor = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::protocol_descriptor" ) ) );
const auto compute_bandwidth_registry = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::compute_bandwidth_registry" ) ) );
const auto block_hash_code = util::converter::as< std::string >(
  *crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::block_hash_code" ) ) );

} // namespace key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
constexpr uint32_t max_object_size = 512;

} // namespace system_call_dispatch

constexpr uint32_t max_object_size = 1'024 * 1'024; // 1 MB

struct genesis_entry
{
  state_db::object_space space;
  state_db::object_key key;
  state_db::object_value value;
};

using genesis_data = std::vector< genesis_entry >;

struct head
{
  protocol::digest id{ std::byte{ 0x00 } };
  uint64_t height = 0;
  protocol::digest previous{ std::byte{ 0x00 } };
  uint64_t last_irreversible_block = 0;
  protocol::digest state_merkle_root{ std::byte{ 0x00 } };
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
