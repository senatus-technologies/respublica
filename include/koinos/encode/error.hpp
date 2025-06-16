#pragma once

#include <expected>
#include <system_error>

namespace koinos::encode {

// NOLINTBEGIN(performance-enum-size)

enum class encode_errc : int
{
  ok = 0,
  invalid_character,
  invalid_length
};

// NOLINTEND(performance-enum-size)

struct encode_category final: std::error_category
{
  const char* name() const noexcept override;
  std::string message( int condition ) const noexcept override;
};

std::error_code make_error_code( encode_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::encode

template<>
struct std::is_error_code_enum< koinos::encode::encode_errc >: public std::true_type
{};
