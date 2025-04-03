#include <koinos/vm_manager/iwasm/exceptions.hpp>
#include <koinos/vm_manager/iwasm/module_cache.hpp>

namespace koinos::vm_manager::iwasm {

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

module_ptr module_cache::create_module( const std::string& id, const std::string& bytecode )
{
  char errbuf[ 128 ] = { '\0' };
  std::string bytecode_copy = bytecode;

  auto module = wasm_runtime_load( reinterpret_cast< uint8_t* >( const_cast< char* >( bytecode_copy.data() ) ),
                                   bytecode_copy.size(),
                                   errbuf,
                                   sizeof( errbuf ) );

  KOINOS_ASSERT( module, module_parse_exception, "iwasm error loading module, ${msg}", ( "msg", errbuf ) );

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
  auto ptr = std::make_shared< const module_guard >( module, std::move( bytecode_copy ) );
  _module_map[ id ] = std::make_pair( ptr, _lru_list.begin() );

  return ptr;
}

module_ptr module_cache::get_or_create_module( const std::string& id, const std::string& bytecode )
{
  std::lock_guard< std::mutex > lock( _mutex );

  if( auto module = get_module( id ); module )
    return module;

  return create_module( id, bytecode );
}

void module_cache::clear()
{
  _module_map.clear();
  _lru_list.clear();
}

} // namespace koinos::vm_manager::iwasm
