#include <koinos/chain/state.hpp>

namespace koinos::chain::state {
namespace space {

enum class system_space_id : uint32_t
{
  metadata          = 0,
  contract_bytecode = 1,
  contract_metadata = 2,
  transaction_nonce = 3
};

const state_db::object_space& program_bytecode()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::contract_bytecode };
  return s;
}

const state_db::object_space& program_metadata()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::contract_metadata };
  return s;
}

const state_db::object_space& metadata()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::metadata };
  return s;
}

const state_db::object_space& transaction_nonce()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::transaction_nonce };
  return s;
}

} // namespace space

namespace key {

using namespace std::string_literals;

state_db::key_type head_block()
{
  static auto h = crypto::hash( "object_key::head_block"s );
  return h;
}

state_db::key_type chain_id()
{
  static auto h = crypto::hash( "object_key::chain_id"s );
  return h;
}

state_db::key_type genesis_key()
{
  static auto h = crypto::hash( "object_key::genesis_key"s );
  return h;
}

state_db::key_type resource_limit_data()
{
  static auto h = crypto::hash( "object_key::resource_limit_data"s );
  return h;
}

state_db::key_type max_account_resources()
{
  static auto h = crypto::hash( "object_key::max_account_resources"s );
  return h;
}

state_db::key_type protocol_descriptor()
{
  static auto h = crypto::hash( "object_key::protocol_descriptor"s );
  return h;
}

state_db::key_type compute_bandwidth_registry()
{
  static auto h = crypto::hash( "object_key::compute_bandwidth_registry"s );
  return h;
}

state_db::key_type block_hash_code()
{
  static auto h = crypto::hash( "object_key::block_hash_code"s );
  return h;
}

} // namespace key
} // namespace koinos::chain::state
