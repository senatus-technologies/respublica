#include <koinos/vm/module_cache.hpp>

namespace koinos::vm {

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
  std::lock_guard< std::mutex > lock( _mutex );

  auto itr = _module_map.find( id );
  if( itr == _module_map.end() )
    return module_ptr();

  // Erase the entry from the list and push front
  auto ptr = itr->second.first;
  _lru_list.emplace_front( std::move( *( itr->second.second ) ) );
  _lru_list.erase( itr->second.second );

  _module_map.erase( itr );
  _module_map.insert_or_assign( std::span< const std::byte >( _lru_list.front() ),
                                std::make_pair( ptr, _lru_list.begin() ) );

  return ptr;
}

void module_cache::put_module( std::span< const std::byte > id, const module_ptr& module )
{
  std::lock_guard< std::mutex > lock( _mutex );

  // If the cache is full, remove the last entry from the map, free the fizzy module, and pop back
  if( _lru_list.size() >= _cache_size )
  {
    auto it = _module_map.find( std::span< const std::byte >( _lru_list.back() ) );
    if( it != _module_map.end() )
      _module_map.erase( it );

    _lru_list.pop_back();
  }

  _lru_list.emplace_front( id.begin(), id.end() );
  _module_map.insert_or_assign( std::span< const std::byte >( _lru_list.front() ),
                                std::make_pair( module, _lru_list.begin() ) );
}

} // namespace koinos::vm
