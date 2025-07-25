#pragma once

#include <fizzy/fizzy.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace respublica::vm {

constexpr std::size_t default_module_cache_size = 32;

class module
{
private:
  const FizzyModule* _module;

public:
  module( const FizzyModule* m ) :_module( m ) {}

  module( const module& ) = delete;
  module( module&& )      = delete;

  ~module()
  {
    fizzy_free_module( _module );
  }

  module& operator=( const module& ) = delete;
  module& operator=( module&& )      = delete;

  const FizzyModule* get() const
  {
    return _module;
  }
};

class module_cache
{
private:
  using lru_list_type = std::list< std::vector< std::byte > >;
  using module_map_type = std::map< std::span< const std::byte >,
                                    std::pair< std::shared_ptr< module >, typename lru_list_type::iterator >,
                                    decltype( []( std::span< const std::byte > rhs, std::span< const std::byte > lhs ) -> bool
                                    {
                                      return std::ranges::lexicographical_compare( rhs, lhs );
                                    }) >;

  lru_list_type _lru_list;
  module_map_type _module_map;
  std::mutex _mutex;
  const std::size_t _cache_size;

public:
  module_cache( std::size_t size = default_module_cache_size );
  module_cache( const module_cache& ) = delete;
  module_cache( module_cache&& )      = delete;

  ~module_cache();

  module_cache& operator=( const module_cache& ) = delete;
  module_cache& operator=( module_cache&& )      = delete;

  std::shared_ptr< module > get_module( std::span< const std::byte > id );
  void put_module( std::span< const std::byte > id, const std::shared_ptr< module >& module );
};

} // namespace respublica::vm
