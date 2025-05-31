#include <koinos/state_db/backends/map/map_backend.hpp>

namespace koinos::state_db::backends::map {

map_backend::map_backend():
    abstract_backend()
{}

map_backend::map_backend( uint64_t revision, state_node_id id, protocol::block_header header ):
    abstract_backend( revision, id, header )
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

void map_backend::put( std::vector< std::byte >&& key, value_type value )
{
  _map.insert_or_assign( std::move( key ), map_type::mapped_type( value.begin(), value.end() ) );
}

void map_backend::put( std::vector< std::byte >&& key, std::vector< std::byte >&& value )
{
  _map.insert_or_assign( std::move( key ), std::move( value ) );
}

std::optional< value_type > map_backend::get( const std::vector< std::byte >& key ) const
{
  if( auto itr = _map.find( key ); itr != _map.end() )
    return value_type( itr->second );

  return {};
}

void map_backend::erase( const std::vector< std::byte >& key )
{
  _map.erase( key );
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
