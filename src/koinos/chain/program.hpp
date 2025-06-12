#pragma once

#include <koinos/chain/system_interface.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/error/error.hpp>

namespace koinos::chain {

struct program
{
  program() = default;
  program( const program& ) = delete;
  program( program&& ) = delete;
  virtual ~program() = default;

  program& operator =( const program& ) = delete;
  program& operator =( program&& ) = delete;

  virtual error start( system_interface* system, uint32_t entry_point, std::span< const std::span< const std::byte > >& args ) = 0;
};

} // namespace koinos::chain
