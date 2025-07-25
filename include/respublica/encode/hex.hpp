#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <respublica/encode/error.hpp>

namespace respublica::encode {

std::string to_hex( std::span< const std::byte > s ) noexcept;
result< std::vector< std::byte > > from_hex( std::string_view sv ) noexcept;

} // namespace respublica::encode
