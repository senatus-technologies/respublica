#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace koinos::state_db {

struct object_space
{
  bool system;
  std::array< uint8_t, 3 > padding{};
  std::array< std::byte, 32 > address{};
  uint32_t id = 0;
};

using state_node_id = std::array< std::byte, 32 >;
using digest        = std::array< std::byte, 32 >;
using key_type      = std::span< const std::byte >;
using value_type    = std::span< const std::byte >;

constexpr state_node_id null_id = {};

} // namespace koinos::state_db
