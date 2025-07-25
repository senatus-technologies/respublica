#include <respublica/state_db/backends/iterator.hpp>

#include <algorithm>

namespace respublica::state_db::backends {

iterator::iterator( std::unique_ptr< abstract_iterator > itr ):
    _itr( std::move( itr ) )
{}

iterator::iterator( const iterator& other ):
    _itr( other._itr->copy() )
{}

iterator::iterator( iterator&& other ) noexcept:
    _itr( std::move( other._itr ) )
{}

const std::pair< const std::vector< std::byte >, std::vector< std::byte > >& iterator::operator*() const
{
  return **_itr;
}

const std::pair< const std::vector< std::byte >, std::vector< std::byte > >* iterator::operator->() const
{
  return &**_itr;
}

std::pair< std::vector< std::byte >, std::vector< std::byte > > iterator::release()
{
  return _itr->release();
}

iterator& iterator::operator++()
{
  ++( *_itr );
  return *this;
}

iterator& iterator::operator--()
{
  --( *_itr );
  return *this;
}

iterator& iterator::operator=( iterator&& other ) noexcept
{
  _itr = std::move( other._itr );
  return *this;
}

bool iterator::valid() const
{
  return _itr && _itr->valid();
}

bool operator==( const iterator& x, const iterator& y )
{
  if( x.valid() && y.valid() )
  {
    return std::ranges::equal( x->first, y->first );
  }

  return x.valid() == y.valid();
}

bool operator!=( const iterator& x, const iterator& y )
{
  return !( x == y );
}

} // namespace respublica::state_db::backends
