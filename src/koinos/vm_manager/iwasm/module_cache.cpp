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

module_ptr module_cache::get( const std::string& id )
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

module_ptr module_cache::create( const std::string& id, const std::string& bytecode )
{
  char errbuf[ 128 ] = { '\0' };
  std::vector< uint8_t > bytecode_copy( bytecode.begin(), bytecode.end() );

  auto module = wasm_runtime_load( bytecode_copy.data(),
                                    bytecode_copy.size(),
                                    errbuf,
                                    sizeof( errbuf ) );

  KOINOS_ASSERT( module, module_parse_exception, "iwasm error loading module, ${msg}", ( "msg", errbuf ) );
  auto ptr = std::make_shared< const module_guard >( module, std::move( bytecode_copy ) );

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
  _module_map[ id ] = std::make_pair( ptr, _lru_list.begin() );

  return ptr;
}

module_ptr module_cache::get_or_create( const std::string& id, const std::string& bytecode )
{
  std::lock_guard< std::mutex > lock( _mutex );

  if( auto module = get( id ); module )
    return module;

  return create( id, bytecode );
}

} // namespace koinos::vm_manager::iwasm
