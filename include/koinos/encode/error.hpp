#pragma once

#include <expected>
#include <system_error>

namespace koinos::encode {

enum class encode_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  invalid_character,
  invalid_length
};

struct encode_category final: std::error_category
{
  const char* name() const noexcept final;
  std::string message( int condition ) const noexcept final;
};

std::error_code make_error_code( encode_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::encode

template<>
struct std::is_error_code_enum< koinos::encode::encode_errc >: public std::true_type
{};
