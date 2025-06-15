#pragma once

#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <koinos/encode/error.hpp>

namespace koinos::encode {

std::string to_hex( std::span< const std::byte > s ) noexcept;
result< std::vector< std::byte > > from_hex( std::string_view sv ) noexcept;

} // namespace koinos::encode
