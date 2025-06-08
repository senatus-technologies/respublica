#include <koinos/chain/state.hpp>
#include <utility>

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
  static state_db::object_space s{ .system = true, .id = std::to_underlying( system_space_id::contract_bytecode ) };
  return s;
}

const state_db::object_space& program_metadata()
{
  static state_db::object_space s{ .system = true, .id = std::to_underlying( system_space_id::contract_metadata ) };
  return s;
}

const state_db::object_space& metadata()
{
  static state_db::object_space s{ .system = true, .id = std::to_underlying( system_space_id::metadata ) };
  return s;
}

const state_db::object_space& transaction_nonce()
{
  static state_db::object_space s{ .system = true, .id = std::to_underlying( system_space_id::transaction_nonce ) };
  return s;
}

} // namespace space

namespace key {

using namespace std::string_literals;

std::span< const std::byte > head_block()
{
  static auto h = crypto::hash( "object_key::head_block" );
  return h;
}

std::span< const std::byte > genesis_key()
{
  static auto h = crypto::hash( "object_key::genesis_key" );
  return h;
}

} // namespace key
} // namespace koinos::chain::state
