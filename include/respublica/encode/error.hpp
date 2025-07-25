#pragma once

#include <expected>
#include <system_error>

namespace respublica::encode {

enum class encode_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  invalid_character,
  invalid_length
};

const std::error_category& encode_category() noexcept;

std::error_code make_error_code( encode_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace respublica::encode

template<>
struct std::is_error_code_enum< respublica::encode::encode_errc >: public std::true_type
{};
