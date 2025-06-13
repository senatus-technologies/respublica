#pragma once

#include <system_error>

#include <koinos/chain/system_interface.hpp>

namespace koinos::chain {

struct program
{
  program()                 = default;
  program( const program& ) = delete;
  program( program&& )      = delete;
  virtual ~program()        = default;

  program& operator=( const program& ) = delete;
  program& operator=( program&& )      = delete;

  virtual std::error_code start( system_interface* system,
                                 std::uint32_t entry_point,
                                 std::span< const std::span< const std::byte > >& args ) = 0;
};

} // namespace koinos::chain
