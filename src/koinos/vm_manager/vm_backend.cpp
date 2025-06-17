
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>

#if 0
#  include <koinos/vm_manager/iwasm/iwasm_vm_backend.hpp>
#endif

namespace koinos::vm_manager {

std::vector< std::shared_ptr< vm_backend > > get_vm_backends()
{
  std::vector< std::shared_ptr< vm_backend > > result;

  result.push_back( std::make_shared< vm_manager::fizzy::fizzy_vm_backend >() );
#if 0
  result.push_back( std::make_shared< vm_manager::iwasm::iwasm_vm_backend >() );
#endif
  return result;
}

std::string get_default_vm_backend_name()
{
  return "fizzy";
}

std::shared_ptr< vm_backend > get_vm_backend( const std::string& name )
{
  std::vector< std::shared_ptr< vm_backend > > backends = get_vm_backends();
  for( std::shared_ptr< vm_backend > b: backends )
  {
    if( b->backend_name() == name )
      return b;
  }
  return std::shared_ptr< vm_backend >();
}

} // namespace koinos::vm_manager
