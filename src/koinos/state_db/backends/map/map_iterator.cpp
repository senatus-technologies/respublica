#include <koinos/state_db/backends/map/map_iterator.hpp>

#include <stdexcept>

namespace koinos::state_db::backends::map {

map_iterator::map_iterator( std::unique_ptr< iterator_type > itr, const map_type& map ):
    _itr( std::move( itr ) ),
    _map( map )
{}

map_iterator::~map_iterator() {}

value_type map_iterator::operator*() const
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  return value_type( ( *_itr )->second );
}

key_type map_iterator::key() const
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  return key_type( ( *_itr )->first );
}

abstract_iterator& map_iterator::operator++()
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  ++( *_itr );
  return *this;
}

abstract_iterator& map_iterator::operator--()
{
  if( *_itr == _map.begin() )
    throw std::runtime_error( "iterator operation is invalid" );

  --( *_itr );
  return *this;
}

bool map_iterator::valid() const
{
  return _itr && *_itr != _map.end();
}

std::unique_ptr< abstract_iterator > map_iterator::copy() const
{
  return std::make_unique< map_iterator >( std::make_unique< iterator_type >( *_itr ), _map );
}

} // namespace koinos::state_db::backends::map
