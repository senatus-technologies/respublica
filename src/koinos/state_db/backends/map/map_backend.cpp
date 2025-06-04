#include <koinos/state_db/backends/map/map_backend.hpp>

namespace koinos::state_db::backends::map {

map_backend::map_backend():
    abstract_backend()
{}

map_backend::map_backend( const state_node_id& id, uint64_t revision ):
    abstract_backend( id, revision )
{}

map_backend::~map_backend() {}

iterator map_backend::begin() noexcept
{
  return iterator( std::make_unique< map_iterator >( std::make_unique< iterator_type >( _map.begin() ), _map ) );
}

iterator map_backend::end() noexcept
{
  return iterator( std::make_unique< map_iterator >( std::make_unique< iterator_type >( _map.end() ), _map ) );
}

int64_t map_backend::put( std::vector< std::byte >&& key, std::span< const std::byte > value )
{
  return put( std::move( key ), std::vector< std::byte >( value.begin(), value.end() ) );
}

int64_t map_backend::put( std::vector< std::byte >&& key, std::vector< std::byte >&& value )
{
  int64_t size = key.size() + value.size();
  auto itr = _map.lower_bound( key );

  if( std::ranges::equal( key, itr->first ) )
    size -= itr->second.size();

  _map.insert_or_assign( itr, std::move( key ), std::move( value ) );

  return size;
}

std::optional< std::span< const std::byte > > map_backend::get( const std::vector< std::byte >& key ) const
{
  if( auto itr = _map.find( key ); itr != _map.end() )
    return std::span< const std::byte >( itr->second );

  return {};
}

int64_t map_backend::remove( const std::vector< std::byte >& key )
{
  int64_t size = 0;

  if( auto itr = _map.find( key ); itr != _map.end() )
  {
    size -= itr->first.size() + itr->second.size();
    _map.erase( itr );
  }

  return size;
}

void map_backend::clear() noexcept
{
  _map.clear();
}

uint64_t map_backend::size() const noexcept
{
  return _map.size();
}

void map_backend::start_write_batch() {}

void map_backend::end_write_batch() {}

void map_backend::store_metadata() {}

std::shared_ptr< abstract_backend > map_backend::clone() const
{
  return std::make_shared< map_backend >( *this );
}

} // namespace koinos::state_db::backends::map
