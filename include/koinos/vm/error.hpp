#pragma once

#include <cstdint>
#include <expected>
#include <system_error>

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

const std::error_category& virtual_machine_category() noexcept;
const std::error_category& program_category() noexcept;

std::error_code make_error_code( virtual_machine_errc e );
std::error_code make_error_code( std::int32_t e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::vm

template<>
struct std::is_error_code_enum< koinos::vm::virtual_machine_errc >: public std::true_type
{};
