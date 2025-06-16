#pragma once

#include <map>
#include <vector>

namespace koinos::state_db::backends::map {

using map_type      = std::map< std::vector< std::byte >, std::vector< std::byte > >;
using iterator_type = map_type::iterator;

} // namespace koinos::state_db::backends::map
