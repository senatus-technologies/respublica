#pragma once

#include <koinos/state_db/bytes_less.hpp>

#include <map>
#include <span>
#include <vector>

namespace koinos::state_db::backends::map {

using map_type      = std::map< std::vector< std::byte >, std::vector< std::byte > >;
using span_type     = std::map< std::span< const std::byte >, map_type::iterator, bytes_less >;
using iterator_type = map_type::iterator;

} // namespace koinos::state_db::backends::map