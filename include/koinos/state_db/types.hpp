#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace koinos::state_db {

enum class fork_resolution_algorithm
{
  fifo,
  block_time,
  pob
};

class state_node;
class permanent_state_node;
class temporary_state_node;
class state_delta;
class delta_index;

struct object_space
{
  bool system;
  std::array< uint8_t, 3 > padding{};
  std::array< std::byte, 32 > address{};
  uint32_t id = 0;
};

using state_node_ptr = std::shared_ptr< state_node >;
using permanent_state_node_ptr = std::shared_ptr< permanent_state_node >;
using temporary_state_node_ptr = std::shared_ptr< temporary_state_node >;
using state_node_id = std::array< std::byte, 32 >;
using genesis_init_function = std::function< void( state_node_ptr ) >;

constexpr state_node_id null_id = {};

} // namespace koinos::state_db
