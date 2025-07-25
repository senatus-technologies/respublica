#include <cassert>

#include <fizzy/fizzy.h>

#include <respublica/memory.hpp>
#include <respublica/vm/error.hpp>
#include <respublica/vm/module_cache.hpp>
#include <respublica/vm/program_context.hpp>
#include <respublica/vm/virtual_machine.hpp>

namespace respublica::vm {

result< std::shared_ptr< module > > parse_bytecode( std::span< const std::byte > bytecode ) noexcept
{
  if( !bytecode.size() )
    return std::unexpected( virtual_machine_errc::invalid_module );

  auto ptr = fizzy_parse( memory::pointer_cast< const std::uint8_t* >( bytecode.data() ), bytecode.size(), nullptr );

  if( !ptr )
    return std::unexpected( virtual_machine_errc::invalid_module );

  return std::make_shared< module >( ptr );
}

result< std::shared_ptr< module > >
make_module( module_cache& cache, std::span< const std::byte > bytecode, std::span< const std::byte > id ) noexcept
{
  assert( id.size() );

  if( auto mod = cache.get_module( id ); mod )
    return mod;

  std::shared_ptr< module > mod;
  if( auto result = parse_bytecode( bytecode ); result )
    mod = std::move( *result );
  else
    return std::unexpected( result.error() );

  cache.put_module( id, mod );
  return mod;
}

virtual_machine::virtual_machine():
    _cache( std::make_unique< module_cache >() )
{}

virtual_machine::~virtual_machine() = default;

std::error_code
virtual_machine::run( host_api& hapi, std::span< const std::byte > bytecode, std::span< const std::byte > id ) noexcept
{
  auto module = make_module( *_cache, bytecode, id );
  if( !module )
    return module.error();

  program_context context( hapi, *module );
  return context.start();
}

} // namespace respublica::vm
