#pragma once

#include <cstddef>
#include <string>

#include <koinos/crypto/crypto.hpp>

namespace koinos::state_db {

struct object_space
{
  bool system;
  std::array< uint8_t, 3 > padding    = { 0x00, 0x00, 0x00 };
  std::array< std::byte, 32 > address = {};
  uint32_t id = 0;
};

using state_node_id = std::array< std::byte, 32 >;
using digest   = std::array< std::byte, 32 >;
using object_key    = std::string;
using object_value  = std::string;

constexpr state_node_id null_id = {};

} // namespace koinos::state_db
