#pragma once

#include <expected>
#include <system_error>

namespace respublica::program {

enum class program_errc : int // NOLINT(performance-enum-size)
{
  ok,
  unauthorized,
  invalid_instruction,
  insufficient_balance,
  insufficient_supply,
  invalid_argument,
  unexpected_object,
  overflow
};

const std::error_category& program_category() noexcept;

std::error_code make_error_code( program_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace respublica::program

template<>
struct std::is_error_code_enum< respublica::program::program_errc >: public std::true_type
{};
