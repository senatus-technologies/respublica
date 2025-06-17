#include <koinos/vm_manager/error.hpp>
#include <koinos/vm_manager/iwasm/module_cache.hpp>

#include <koinos/memory.hpp>

namespace koinos::vm_manager::iwasm {

module_manager::module_manager( std::span< const std::byte > bytecode ):
    _bytecode( bytecode.begin(), bytecode.end() )
{}

module_manager::~module_manager() noexcept
{
  if( _module != nullptr )
    wasm_runtime_unload( _module );
}

const wasm_module_t module_manager::get() const
{
  return _module;
}

result< module_ptr > module_manager::create( std::span< const std::byte > bytecode )
{
  constexpr std::size_t error_size = 128;
  std::array< char, error_size > error_buf{ '\0' };

  auto m_ptr = std::shared_ptr< module_manager >( new module_manager( bytecode ) );

  auto wasm_module = wasm_runtime_load( memory::pointer_cast< std::uint8_t* >( m_ptr->_bytecode.data() ),
                                        m_ptr->_bytecode.size(),
                                        error_buf.data(),
                                        error_buf.size() );
  if( wasm_module == nullptr )
    return std::unexpected( virtual_machine_errc::load_failure );

  m_ptr->_module = wasm_module;

  return m_ptr;
}

module_cache::module_cache( std::size_t size ):
    _cache_size( size )
{}

module_cache::~module_cache()
{
  std::lock_guard< std::mutex > lock( _mutex );
  _module_map.clear();
}

module_ptr module_cache::get_module( std::span< const std::byte > id )
{
  auto itr = _module_map.find( id );
  if( itr == _module_map.end() )
    return module_ptr();

  // Erase the entry from the list and push front
  auto mod = itr->second.first;
  _lru_list.emplace_front( std::move( *( itr->second.second ) ) );
  _lru_list.erase( itr->second.second );

  _module_map.erase( itr );
  _module_map.insert_or_assign( std::span< const std::byte >( _lru_list.front() ),
                                std::make_pair( mod, _lru_list.begin() ) );

  return mod;
}

result< module_ptr > module_cache::create_module( std::span< const std::byte > id,
                                                  std::span< const std::byte > bytecode )
{
  auto mod = module_manager::create( bytecode );
  if( !mod )
    return mod;

  // If the cache is full, remove the last entry from the map, free the iwasm module, and pop back
  if( _lru_list.size() >= _cache_size )
  {
    auto it = _module_map.find( std::span< const std::byte >( _lru_list.back() ) );
    if( it != _module_map.end() )
      _module_map.erase( it );

    _lru_list.pop_back();
  }

  _lru_list.emplace_front( id.begin(), id.end() );
  _module_map.insert_or_assign( std::span< const std::byte >( _lru_list.front() ),
                                std::make_pair( mod.value(), _lru_list.begin() ) );

  return mod;
}

result< module_ptr > module_cache::get_or_create_module( std::span< const std::byte > id,
                                                         std::span< const std::byte > bytecode )
{
  std::lock_guard< std::mutex > lock( _mutex );

  if( auto mod = get_module( id ); mod )
    return mod;

  return create_module( id, bytecode );
}

void module_cache::clear()
{
  _module_map.clear();
  _lru_list.clear();
}

} // namespace koinos::vm_manager::iwasm
