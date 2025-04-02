#include <wasm_export.h>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace koinos::vm_manager::iwasm {

class module_cache;

class module_guard
{
private:
  wasm_module_t _module = nullptr;
  std::string   _bytecode;

public:
  module_guard( wasm_module_t module, std::string&& bytecode ):
    _module( module ),
    _bytecode( std::move( bytecode ) )
  {}

  ~module_guard()
  {
    if( _module )
      wasm_runtime_unload( _module );
  }

  const wasm_module_t get() const
  {
    return _module;
  }
};

using module_ptr = std::shared_ptr< const module_guard >;

class module_cache
{
private:
  using lru_list_type = std::list< std::string >;

  using module_map_type = std::map< std::string, std::pair< module_ptr, typename lru_list_type::iterator > >;

  lru_list_type _lru_list;
  module_map_type _module_map;
  std::mutex _mutex;
  const std::size_t _cache_size;

  module_ptr get_module( const std::string& id );
  module_ptr create_module( const std::string& id, const std::string& bytecode );

public:
  module_cache( std::size_t size );
  ~module_cache();

  module_ptr get_or_create_module( const std::string& id, const std::string& bytecode );
  void clear();
};

} // namespace koinos::vm_manager::iwasm
