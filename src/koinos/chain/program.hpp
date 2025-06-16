#pragma once

#include <system_error>

#include <koinos/controller/system_interface.hpp>

namespace koinos::controller {

struct program
{
  program()                 = default;
  program( const program& ) = delete;
  program( program&& )      = delete;
  virtual ~program()        = default;

  program& operator=( const program& ) = delete;
  program& operator=( program&& )      = delete;

  virtual std::error_code start( system_interface* system,
                                 const std::span< const std::span< const std::byte > > args ) = 0;
};

} // namespace koinos::controller
