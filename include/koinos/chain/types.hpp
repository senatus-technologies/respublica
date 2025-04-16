#pragma once

#include <koinos/error/error.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace koinos::chain {

using std::make_pair;
using std::map;
using std::pair;
using std::string;
using std::vector;

using bytes_s = std::span< const std::byte >;
using bytes_v = std::vector< std::byte >;

using error::error;

} // namespace koinos::chain
