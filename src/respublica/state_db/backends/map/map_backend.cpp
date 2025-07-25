#include <respublica/state_db/backends/map/map_backend.hpp>

namespace respublica::state_db::backends::map {

map_backend::map_backend():
    abstract_backend()
{}

map_backend::map_backend( const state_node_id& id, std::uint64_t revision ):
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

std::int64_t map_backend::put( std::vector< std::byte >&& key, std::vector< std::byte >&& value )
{
  std::int64_t size = std::ssize( value );
  auto itr          = _map.lower_bound( key );

  if( itr != _map.end() && std::ranges::equal( key, itr->first ) )
    size -= std::ssize( itr->second );
  else
    size += std::ssize( key );

  _map.insert_or_assign( itr, std::move( key ), std::move( value ) );

  return size;
}

std::optional< std::span< const std::byte > > map_backend::get( const std::vector< std::byte >& key ) const
{
  if( auto itr = _map.find( key ); itr != _map.end() )
    return std::span< const std::byte >( itr->second );

  return {};
}

std::int64_t map_backend::remove( const std::vector< std::byte >& key )
{
  std::int64_t size = 0;

  if( auto itr = _map.find( key ); itr != _map.end() )
  {
    size -= std::ssize( itr->first ) + std::ssize( itr->second );
    _map.erase( itr );
  }

  return size;
}

void map_backend::clear() noexcept
{
  _map.clear();
}

std::uint64_t map_backend::size() const noexcept
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

} // namespace respublica::state_db::backends::map
