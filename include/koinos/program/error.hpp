#pragma once

#include <expected>
#include <system_error>

namespace koinos::program {

enum class program_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  failure
};

struct program_category final: std::error_category
{
  const char* name() const noexcept final;
  std::string message( int condition ) const noexcept final;
};

std::error_code make_error_code( program_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::program

template<>
struct std::is_error_code_enum< koinos::program::program_errc >: public std::true_type
{};
