#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <utility>

namespace koinos::vm {

enum class virtual_machine_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  trapped,
  invalid_arguments,
  execution_environment_failure,
  function_lookup_failure,
  load_failure,
  instantiate_failure,
  invalid_pointer,
  invalid_module,
  invalid_context,
  entry_point_not_found
};

struct virtual_machine_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "virtual machine";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< virtual_machine_errc >( condition ) )
    {
      case virtual_machine_errc::ok:
        return "ok"s;
      case virtual_machine_errc::trapped:
        return "trapped"s;
      case virtual_machine_errc::invalid_arguments:
        return "invalid arguments"s;
      case virtual_machine_errc::execution_environment_failure:
        return "execution environment failure"s;
      case virtual_machine_errc::function_lookup_failure:
        return "function lookup failure"s;
      case virtual_machine_errc::load_failure:
        return "load failure"s;
      case virtual_machine_errc::instantiate_failure:
        return "instantiate failure"s;
      case virtual_machine_errc::invalid_pointer:
        return "invalid pointer"s;
      case virtual_machine_errc::invalid_module:
        return "invalid module"s;
      case virtual_machine_errc::invalid_context:
        return "invalid context"s;
      case virtual_machine_errc::entry_point_not_found:
        return "entry point not found"s;
    }
    std::unreachable();
  }
};

struct virtual_machine_program_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "virtual machine program";
  }

  std::string message( int condition ) const final
  {
    return "program exited with code " + std::to_string( condition );
  }
};

std::error_code make_error_code( virtual_machine_errc e );
std::error_code make_error_code( std::int32_t e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::vm

template<>
struct std::is_error_code_enum< koinos::vm::virtual_machine_errc >: public std::true_type
{};
