#pragma once

#include <algorithm>
#include <span>
#include <vector>

namespace koinos::vm_manager {

struct bytes_less
{
  bool operator()( std::span< const std::byte > rhs, std::span< const std::byte > lhs ) const
  {
    return std::ranges::lexicographical_compare( rhs, lhs );
  }
};

} // namespace koinos::vm_manager
