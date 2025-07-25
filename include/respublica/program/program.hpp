#pragma once

#include <string>
#include <system_error>

#include <respublica/program/system_interface.hpp>

namespace respublica::program {

struct program
{
  program()                 = default;
  program( const program& ) = delete;
  program( program&& )      = delete;
  virtual ~program()        = default;

  program& operator=( const program& ) = delete;
  program& operator=( program&& )      = delete;

  virtual std::error_code run( system_interface* system, std::span< const std::string > arguments ) = 0;
};

} // namespace respublica::program
