#pragma once

#include <cstddef>
#include <string>

#include <koinos/chain/chain.pb.h>
#include <koinos/crypto/multihash.hpp>

namespace koinos::state_db {

struct object_space
{
  bool system = false;
  std::array< std::byte, 32 > address = { std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
                                          std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
                                          std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
                                          std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 } };
  uint32_t id = 0;

  explicit operator chain::object_space() const;
};

using state_node_id = crypto::multihash;
using object_key    = std::string;
using object_value  = std::string;

} // namespace koinos::state_db
