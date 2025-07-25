#include <respublica/vm/module_cache.hpp>

namespace respublica::vm {

module_cache::module_cache( std::size_t size ):
    _cache_size( size )
{}

module_cache::~module_cache()
{
  std::lock_guard< std::mutex > lock( _mutex );
  _module_map.clear();
}

std::shared_ptr< module > module_cache::get_module( std::span< const std::byte > id )
{
  std::lock_guard< std::mutex > lock( _mutex );

  auto it = _module_map.find( id );
  if( it == _module_map.end() )
    return std::shared_ptr< module >();

  if( it->second.second != _lru_list.begin() )
    _lru_list.splice( _lru_list.begin(), _lru_list, it->second.second );

  return it->second.first;
}

void module_cache::put_module( std::span< const std::byte > id, const std::shared_ptr< module >& module )
{
  std::lock_guard< std::mutex > lock( _mutex );

  if( _lru_list.size() >= _cache_size )
  {
    auto it = _module_map.find( std::span< const std::byte >( _lru_list.back() ) );
    if( it != _module_map.end() )
      _module_map.erase( it );

    _lru_list.pop_back();
  }

  _lru_list.emplace_front( id.begin(), id.end() );
  _module_map.insert_or_assign( _lru_list.front(), std::make_pair( module, _lru_list.begin() ) );
}

} // namespace respublica::vm
