#include <koinos/controller/state.hpp>
#include <utility>

namespace koinos::controller::state {
namespace space {

enum class system_space_id : std::uint8_t
{
  metadata          = 0,
  program_data      = 1,
  transaction_nonce = 2
};

const state_db::object_space& program_data()
{
  static state_db::object_space s{ .system = true, .id = std::to_underlying( system_space_id::program_data ) };
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

std::span< const std::byte > genesis_key()
{
  static auto h = crypto::hash( "object_key::genesis_key" );
  return h;
}

} // namespace key
} // namespace koinos::controller::state
