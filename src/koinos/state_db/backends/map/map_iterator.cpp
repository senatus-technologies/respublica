#include <koinos/state_db/backends/map/map_iterator.hpp>

#include <stdexcept>

namespace koinos::state_db::backends::map {

map_iterator::map_iterator( std::unique_ptr< std::map< detail::key_type, detail::value_type >::iterator > itr,
                            const std::map< detail::key_type, detail::value_type >& map ):
    _itr( std::move( itr ) ),
    _map( map )
{}

map_iterator::~map_iterator() {}

const map_iterator::value_type& map_iterator::operator*() const
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  return ( *_itr )->second;
}

const map_iterator::key_type& map_iterator::key() const
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  return ( *_itr )->first;
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
  return std::make_unique< map_iterator >(
    std::make_unique< std::map< detail::key_type, detail::value_type >::iterator >( *_itr ),
    _map );
}

} // namespace koinos::state_db::backends::map
