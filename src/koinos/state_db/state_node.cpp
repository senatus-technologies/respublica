#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/state_node.hpp>

namespace koinos::state_db {

inline std::vector< std::byte > make_compound_key( const object_space& space, std::span< const std::byte > key )
{
  std::vector< std::byte > compound_key;
  compound_key.reserve( sizeof( space ) + key.size() );
  std::ranges::copy( std::span< const std::byte >( reinterpret_cast< const std::byte* >( &space ), sizeof( space ) ),
                     std::back_inserter( compound_key ) );
  std::ranges::copy( key, std::back_inserter( compound_key ) );
  return compound_key;
}

state_node::state_node( std::shared_ptr< state_delta > delta ):
    _delta( delta )
{}

state_node::~state_node() {}

std::optional< std::span< const std::byte > > state_node::get( const object_space& space,
                                                               std::span< const std::byte > key ) const
{
  return _delta->get( make_compound_key( space, key ) );
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
  if( _delta->final() )
    throw std::runtime_error( "cannot write to a finalized node" );

  return _delta->put( make_compound_key( space, key ), value );
}

int64_t state_node::remove( const object_space& space, std::span< const std::byte > key )
{
  if( _delta->final() )
    throw std::runtime_error( "cannot write to a finalized node" );

  return _delta->remove( make_compound_key( space, key ) );
}

std::shared_ptr< temporary_state_node > state_node::make_child() const
{
  return std::make_shared< temporary_state_node >( _delta->make_child() );
}

std::shared_ptr< temporary_state_node > state_node::clone() const
{
  return std::make_shared< temporary_state_node >( _delta->clone() );
}

const state_node_id& state_node::id() const
{
  return _delta->id();
}

const state_node_id& state_node::parent_id() const
{
  return _delta->parent_id();
}

uint64_t state_node::revision() const
{
  return _delta->revision();
}

} // namespace koinos::state_db
