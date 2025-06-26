#include <koinos/vm_manager/error.hpp>

namespace koinos::vm_manager {

std::error_code make_error_code( virtual_machine_errc e )
{
  static auto category = virtual_machine_category{};
  return std::error_code( static_cast< int >( e ), category );
}

std::error_code make_error_code( std::int32_t e )
{
  static auto category = virtual_machine_program_category{};
  return std::error_code( static_cast< int >( e ), category );
}

} // namespace koinos::vm_manager
