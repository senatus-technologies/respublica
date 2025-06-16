#include <koinos/vm_manager/bytes_less.hpp>

#include <fizzy/fizzy.h>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace koinos::vm_manager::fizzy {

class module_guard
{
private:
  const FizzyModule* _module;

public:
  module_guard( const FizzyModule* m ):
      _module( m )
  {}

  module_guard( const module_guard& ) = delete;
  module_guard( module_guard&& )      = delete;

  ~module_guard()
  {
    fizzy_free_module( _module );
  }

  module_guard& operator=( const module_guard& ) = delete;
  module_guard& operator=( module_guard&& )      = delete;

  const FizzyModule* get() const
  {
    return _module;
  }
};

using module_ptr = std::shared_ptr< const module_guard >;

class module_cache
{
private:
  using lru_list_type = std::list< std::vector< std::byte > >;
  using module_map_type =
    std::map< std::span< const std::byte >, std::pair< module_ptr, typename lru_list_type::iterator >, bytes_less >;

  lru_list_type _lru_list;
  module_map_type _module_map;
  std::mutex _mutex;
  const std::size_t _cache_size;

public:
  module_cache( std::size_t size = 32 );
  module_cache( const module_cache& ) = delete;
  module_cache( module_cache&& )      = delete;

  ~module_cache();

  module_cache& operator=( const module_cache& ) = delete;
  module_cache& operator=( module_cache&& )      = delete;

  module_ptr get_module( std::span< const std::byte > id );
  void put_module( std::span< const std::byte > id, const module_ptr& module );
};

} // namespace koinos::vm_manager::fizzy
