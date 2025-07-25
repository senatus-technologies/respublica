#include <respublica/state_db/backends/map/map_iterator.hpp>

#include <stdexcept>

namespace respublica::state_db::backends::map {

map_iterator::map_iterator( std::unique_ptr< iterator_type > itr, map_type& map ):
    _itr( std::move( itr ) ),
    _map( map )
{}

map_iterator::~map_iterator() {}

const std::pair< const std::vector< std::byte >, std::vector< std::byte > >& map_iterator::operator*() const
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  return **_itr;
}

std::pair< std::vector< std::byte >, std::vector< std::byte > > map_iterator::release()
{
  if( !valid() )
    throw std::runtime_error( "iterator operation is invalid" );

  auto node = _map.extract( *_itr );
  _itr.reset();

  return std::make_pair( std::move( node.key() ), std::move( node.mapped() ) );
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

} // namespace respublica::state_db::backends::map
