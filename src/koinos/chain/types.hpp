#pragma once

#include <koinos/error/error.hpp>

#include <span>
#include <vector>

namespace koinos::chain {

using bytes_s = std::span< const std::byte >;
using bytes_v = std::vector< std::byte >;

using error::error;

} // namespace koinos::chain
