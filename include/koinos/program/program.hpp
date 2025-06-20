#pragma once

#include <string>
#include <system_error>

#include <koinos/program/program_interface.hpp>

namespace koinos::program {

struct program
{
  program()                 = default;
  program( const program& ) = delete;
  program( program&& )      = delete;
  virtual ~program()        = default;

  program& operator=( const program& ) = delete;
  program& operator=( program&& )      = delete;

  virtual std::error_code start( program_interface* system, std::span< const std::string > arguments ) = 0;
};

} // namespace koinos::program
