#pragma once

#include <expected>
#include <string>
#include <system_error>
#include <utility>

namespace koinos::vm_manager {

// NOLINTBEGIN(performance-enum-size)

enum class virtual_machine_code : int
{
  ok = 0,
  trapped,
  invalid_arguments,
  execution_environment_failure,
  function_lookup_failure,
  load_failure,
  instantiate_failure
};

// NOLINTEND

struct virtual_machine_category: std::error_category
{
  const char* name() const noexcept override
  {
    return "virtual machine";
  }

  std::string message( int condition ) const override
  {
    using namespace std::string_literals;
    switch( static_cast< virtual_machine_code >( condition ) )
    {
      case virtual_machine_code::ok:
        return "ok"s;
      case virtual_machine_code::trapped:
        return "trapped"s;
      case virtual_machine_code::invalid_arguments:
        return "invalid arguments"s;
      case virtual_machine_code::execution_environment_failure:
        return "execution environment failure"s;
      case virtual_machine_code::function_lookup_failure:
        return "function lookup failure"s;
      case virtual_machine_code::load_failure:
        return "load failure"s;
      case virtual_machine_code::instantiate_failure:
        return "instantiate failure"s;
    }
    std::unreachable();
  }
};

std::error_code make_error_code( koinos::vm_manager::virtual_machine_code e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::vm_manager

template<>
struct std::is_error_code_enum< koinos::vm_manager::virtual_machine_code >: public std::true_type
{};
