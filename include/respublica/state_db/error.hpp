#pragma once

#include <cstdint>
#include <expected>
#include <system_error>
#include <type_traits>

namespace respublica::state_db {

enum class state_db_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  parent_not_complete,
  conflicting_parents,
  not_finalized
};

const std::error_category& state_db_category() noexcept;

std::error_code make_error_code( state_db_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace respublica::state_db

template<>
struct std::is_error_code_enum< respublica::state_db::state_db_errc >: public std::true_type
{};
