#include <koinos/state_db/delta_index.hpp>
#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/state_node.hpp>

namespace koinos::state_db {

permanent_state_node::permanent_state_node( std::shared_ptr< state_delta > delta, std::shared_ptr< delta_index > index ):
  state_node( delta ),
  _index( index )
{}

permanent_state_node::~permanent_state_node()
{}

bool permanent_state_node::is_final() const
{
  return _delta->is_finalized();
}

void permanent_state_node::finalize()
{
  _delta->finalize();
  if( auto index = _index.lock(); index )
    index->finalize_delta( _delta );
  else
    throw std::runtime_error( "database is not open" );
}

const crypto::digest& permanent_state_node::merkle_root() const
{
  return _delta->merkle_root();
}

void permanent_state_node::discard()
{
  if( auto index = _index.lock(); index )
    index->remove_delta( _delta );
  else
    throw std::runtime_error( "database is not open" );
}

void permanent_state_node::commit()
{
  if( auto index = _index.lock(); index )
  {
    _delta->commit();
    index->commit_delta( _delta );
  }
  else
    throw std::runtime_error( "database is not open" );
}

std::shared_ptr< permanent_state_node > permanent_state_node::create_child( const state_node_id& child_id, const protocol::block_header& header ) const
{
  auto child = _delta->make_child( child_id, header );
  auto index = _index.lock();

  if( !index )
    throw std::runtime_error( "database is not open" );

  index->add_delta( child );
  return std::make_shared< permanent_state_node >( child, index );
}

std::shared_ptr< permanent_state_node > permanent_state_node::clone( const state_node_id& new_id, const protocol::block_header& header ) const
{
  auto new_delta = _delta->clone( new_id, header );
  auto index = _index.lock();

  if( !index )
    throw std::runtime_error( "database is not open" );

  index->add_delta( new_delta );
  return std::make_shared< permanent_state_node >( new_delta, index );
}

} // namespace koinos::state_db
