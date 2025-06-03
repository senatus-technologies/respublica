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

state_node::state_node( std::shared_ptr< state_delta > delta ) :
  _delta( delta )
{}

state_node::~state_node() {}

std::optional< std::span< const std::byte > > state_node::get_object( const object_space& space, std::span< const std::byte > key ) const
{
  return _delta->find( make_compound_key( space, key ) );
}

std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > > state_node::get_next_object( const object_space& space,
                                                                      std::span< const std::byte > key ) const
{
  return {};
}

std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > > state_node::get_prev_object( const object_space& space,
                                                                      std::span< const std::byte > key ) const
{
  return {};
}

int64_t state_node::put_object( const object_space& space, std::span< const std::byte > key, std::span< const std::byte > value )
{
  if( _delta->is_finalized() )
    throw std::runtime_error( "cannot write to a finalized node" );

  auto compound_key  = make_compound_key( space, key );
  int64_t bytes_used = 0;
  auto pobj          = _delta->find( compound_key );

  if( pobj )
    bytes_used -= pobj->size();
  else
    bytes_used += compound_key.size();

  bytes_used += value.size();
  _delta->put( std::move( compound_key ), value );

  return bytes_used;
}

int64_t state_node::remove_object( const object_space& space, std::span< const std::byte > key )
{
  if( _delta->is_finalized() )
    throw std::runtime_error( "cannot write to a finalized node" );

  auto compound_key  = make_compound_key( space, key );
  int64_t bytes_used = 0;
  auto pobj          = _delta->find( compound_key );

  if( pobj )
  {
    bytes_used -= pobj->size();
    bytes_used -= compound_key.size();
  }

  _delta->erase( std::move( compound_key ) );

  return bytes_used;
}

std::shared_ptr< temporary_state_node > state_node::create_child() const
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

const protocol::block_header& state_node::block_header() const
{
  return _delta->block_header();
}

} // namespace koinos::state_db
