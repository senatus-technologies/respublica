#include <koinos/vm_manager/error.hpp>

namespace koinos::vm_manager {

std::error_code make_error_code( koinos::vm_manager::virtual_machine_code e )
{
  static auto category = koinos::vm_manager::virtual_machine_category{};
  return std::error_code( static_cast< int >( e ), category );
}

} // namespace koinos::vm_manager
