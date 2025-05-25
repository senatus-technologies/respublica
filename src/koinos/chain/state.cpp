#include <koinos/chain/object_spaces.pb.h>
#include <koinos/chain/state.hpp>

namespace koinos::chain { namespace state { namespace space {

enum class system_space_id : uint32_t
{
  metadata          = 0,
  contract_bytecode = 1,
  contract_metadata = 2,
  transaction_nonce = 3
};

const state_db::object_space program_bytecode()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::contract_bytecode };
  return s;
}

const state_db::object_space program_metadata()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::contract_metadata };
  return s;
}

const state_db::object_space metadata()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::metadata };
  return s;
}

const state_db::object_space transaction_nonce()
{
  static state_db::object_space s{ .system = true, .id = (uint32_t)system_space_id::transaction_nonce };
  return s;
}

}}} // namespace koinos::chain::state::space
