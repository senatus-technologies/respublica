#include <koinos/vm_manager/iwasm/module_cache.hpp>

#include <koinos/log.hpp>

namespace koinos::vm_manager::iwasm {

using koinos::error::error_code;

module_manager::module_manager( const std::string& bytecode ):
    _bytecode( bytecode )
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

std::expected< module_ptr, error > module_manager::create( const std::string& bytecode )
{
  char error_buf[ 128 ] = { '\0' };

  auto m_ptr = std::shared_ptr< module_manager >( new module_manager( bytecode ) );

  auto wasm_module = wasm_runtime_load( reinterpret_cast< uint8_t* >( const_cast< char* >( m_ptr->_bytecode.data() ) ),
                                        m_ptr->_bytecode.size(),
                                        error_buf,
                                        sizeof( error_buf ) );
  if( wasm_module == nullptr )
  {
    LOG( info ) << std::string( error_buf );
    return std::unexpected( error_code::reversion );
  }

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

module_ptr module_cache::get_module( const std::string& id )
{
  auto itr = _module_map.find( id );
  if( itr == _module_map.end() )
    return module_ptr();

  // Erase the entry from the list and push front
  _lru_list.erase( itr->second.second );
  _lru_list.push_front( id );
  auto module       = itr->second.first;
  _module_map[ id ] = std::make_pair( module, _lru_list.begin() );

  return module;
}

std::expected< module_ptr, error > module_cache::create_module( const std::string& id, const std::string& bytecode )
{
  auto mod = module_manager::create( bytecode );
  if( !mod )
    return mod;

  // If the cache is full, remove the last entry from the map, free the iwasm module, and pop back
  if( _lru_list.size() >= _cache_size )
  {
    const auto key = _lru_list.back();
    auto it        = _module_map.find( key );
    if( it != _module_map.end() )
      _module_map.erase( it );

    _lru_list.pop_back();
  }

  _lru_list.push_front( id );
  _module_map[ id ] = std::make_pair( mod.value(), _lru_list.begin() );

  return mod;
}

std::expected< module_ptr, error > module_cache::get_or_create_module( const std::string& id,
                                                                       const std::string& bytecode )
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
