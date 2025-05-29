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
  return iterator(
    std::make_unique< map_iterator >( std::make_unique< iterator_type >( _map.begin() ), _map ) );
}

iterator map_backend::end() noexcept
{
  return iterator(
    std::make_unique< map_iterator >( std::make_unique< iterator_type >( _map.end() ), _map ) );
}

void map_backend::put( key_type key, value_type value )
{
  auto res = _map.insert_or_assign( map_type::key_type( key.begin(), key.end() ),
                                    map_type::mapped_type( value.begin(), value.end() ) );
  _span_map.insert_or_assign( key_type( res.first->first ), res.first );
}

std::optional< value_type > map_backend::get( key_type key ) const
{
  if( auto itr = _span_map.find( key ); itr != _span_map.end() )
    return value_type( itr->second->second );

  return {};
}

void map_backend::erase( key_type k )
{
  if( auto span_itr = _span_map.find( k ); span_itr != _span_map.end() )
  {
    _map.erase( span_itr->second );
    _span_map.erase( span_itr );
  }
}

void map_backend::clear() noexcept
{
  _span_map.clear();
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
