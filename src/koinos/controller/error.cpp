#include <koinos/controller/error.hpp>

namespace koinos::controller {

std::error_code make_error_code( reversion_errc e )
{
  static auto category = reversion_category{};
  return std::error_code( static_cast< int >( e ), category );
}

std::error_code make_error_code( controller_errc e )
{
  static auto category = controller_category{};
  return std::error_code( static_cast< int >( e ), category );
}

} // namespace koinos::controller
