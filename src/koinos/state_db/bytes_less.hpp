#pragma once

#include <algorithm>
#include <span>
#include <vector>

namespace koinos::state_db {

struct bytes_less
{
  bool operator()( const std::vector< std::byte >& rhs, const std::vector< std::byte >& lhs ) const
  {
    return std::less< std::vector< std::byte > >{}( rhs, lhs );
  }

  bool operator()( const std::vector< std::byte >& rhs, std::span< const std::byte > lhs ) const
  {
    return std::ranges::lexicographical_compare( rhs, lhs );
  }

  bool operator()( std::span< const std::byte > rhs, const std::vector< std::byte >& lhs ) const
  {
    return std::ranges::lexicographical_compare( rhs, lhs );
  }

  bool operator()( std::span< const std::byte > rhs, std::span< const std::byte > lhs ) const
  {
    return std::ranges::lexicographical_compare( rhs, lhs );
  }
};

} // namespace koinos::state_db
