#pragma once

#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace koinos::util {

std::string to_hex( std::span< const std::byte > s );
std::vector< std::byte > from_hex( std::string_view sv );

} // namespace koinos::util
