#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace koinos::encode {

std::string to_base58( std::span< const std::byte > s );
std::vector< std::byte > from_base58( std::string_view sv );

} // namespace koinos::encode
