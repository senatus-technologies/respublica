#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

namespace respublica::state_db {

enum class fork_resolution_algorithm : std::uint_fast8_t
{
  fifo
};

class state_node;
class permanent_state_node;
class temporary_state_node;
class state_delta;
class delta_index;

constexpr std::size_t object_space_padding_size = 3;
constexpr std::size_t address_size              = 32;
constexpr std::size_t state_node_id_size        = 32;
constexpr std::size_t digest_size               = 32;

struct object_space
{
  bool system = false;
  std::array< std::uint8_t, object_space_padding_size > padding{};
  std::array< std::byte, address_size > address{};
  std::uint32_t id = 0;
};

using state_node_ptr           = std::shared_ptr< state_node >;
using permanent_state_node_ptr = std::shared_ptr< permanent_state_node >;
using temporary_state_node_ptr = std::shared_ptr< temporary_state_node >;
using state_node_id            = std::array< std::byte, state_node_id_size >;
using digest                   = std::array< std::byte, digest_size >;
using genesis_init_function    = std::function< void( state_node_ptr& ) >;

constexpr state_node_id null_id = {};

} // namespace respublica::state_db
