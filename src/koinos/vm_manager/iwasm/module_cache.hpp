#include <koinos/vm_manager/bytes_less.hpp>

#include <wasm_export.h>

#include <expected>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <koinos/error/error.hpp>

namespace koinos::vm_manager::iwasm {

using koinos::error::error;

class module_cache;

class module_manager
{
private:
  wasm_module_t _module = nullptr;
  std::vector< std::byte > _bytecode;

  module_manager( std::span< const std::byte > bytecode );

public:
  using module_ptr = std::shared_ptr< const module_manager >;

  ~module_manager() noexcept;

  const wasm_module_t get() const;

  static std::expected< module_ptr, error > create( std::span< const std::byte > bytecode );
};

using module_ptr = module_manager::module_ptr;

class module_cache
{
private:
  using lru_list_type = std::list< std::vector< std::byte > >;
  using module_map_type = std::map< std::span< const std::byte >, std::pair< module_ptr, typename lru_list_type::iterator >, bytes_less >;

  lru_list_type _lru_list;
  module_map_type _module_map;
  std::mutex _mutex;
  const std::size_t _cache_size;

  module_ptr get_module( std::span< const std::byte > id );
  std::expected< module_ptr, error > create_module( std::span< const std::byte > id, std::span< const std::byte > bytecode );

public:
  module_cache( std::size_t size );
  ~module_cache();

  std::expected< module_ptr, error > get_or_create_module( std::span< const std::byte > id, std::span< const std::byte > bytecode );
  void clear();
};

} // namespace koinos::vm_manager::iwasm
