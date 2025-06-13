#include <koinos/chain/error.hpp>

namespace koinos::chain {

std::error_code make_error_code( koinos::chain::reversion_code e )
{
  static auto category = koinos::chain::reversion_category{};
  return std::error_code( static_cast< int >( e ), category );
}

std::error_code make_error_code( koinos::chain::controller_code e )
{
  static auto category = koinos::chain::controller_category{};
  return std::error_code( static_cast< int >( e ), category );
}

} // namespace koinos::chain
