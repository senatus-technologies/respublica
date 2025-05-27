#pragma once

#include <koinos/chain/system_interface.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/error/error.hpp>

namespace koinos::chain {

struct program
{
  virtual ~program()                                                                                        = default;
  virtual error start( system_interface* system, uint32_t entry_point, const std::vector< std::span< const std::byte > >& args ) = 0;
};

} // namespace koinos::chain
