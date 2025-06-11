#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/state_node.hpp>

namespace koinos::state_db {

inline std::vector< std::byte > make_compound_key( const object_space& space, std::span< const std::byte > key )
{
  std::vector< std::byte > compound_key;
  compound_key.reserve( sizeof( space ) + key.size() );
  std::ranges::copy( std::as_bytes( std::span( &space, 1 ) ),
                     std::back_inserter( compound_key ) );
  std::ranges::copy( key, std::back_inserter( compound_key ) );
  return compound_key;
}

std::optional< std::span< const std::byte > > state_node::get( const object_space& space,
                                                               std::span< const std::byte > key ) const
{
  return delta()->get( make_compound_key( space, key ) );
}

std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > >
state_node::next( const object_space& space, std::span< const std::byte > key ) const
{
  return {};
}

std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > >
state_node::previous( const object_space& space, std::span< const std::byte > key ) const
{
  return {};
}

int64_t
state_node::put( const object_space& space, std::span< const std::byte > key, std::span< const std::byte > value )
{
  return mutable_delta()->put( make_compound_key( space, key ), value );
}

int64_t state_node::remove( const object_space& space, std::span< const std::byte > key )
{
  return mutable_delta()->remove( make_compound_key( space, key ) );
}

std::shared_ptr< temporary_state_node > state_node::make_child()
{
  return std::make_shared< temporary_state_node >( mutable_delta()->make_child() );
}

std::shared_ptr< temporary_state_node > state_node::clone() const
{
  return std::make_shared< temporary_state_node >( delta()->clone() );
}

const state_node_id& state_node::id() const
{
  return delta()->id();
}

const state_node_id& state_node::parent_id() const
{
  return delta()->parent_id();
}

uint64_t state_node::revision() const
{
  return delta()->revision();
}

} // namespace koinos::state_db
