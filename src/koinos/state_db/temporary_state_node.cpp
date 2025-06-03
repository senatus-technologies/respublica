#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/state_node.hpp>

namespace koinos::state_db {

temporary_state_node::temporary_state_node( std::shared_ptr< state_delta > delta ):
  state_node( delta )
{}

temporary_state_node::~temporary_state_node()
{}

void temporary_state_node::squash()
{
  if( !_delta )
    throw std::runtime_error( "state node has already been squashed" );

  _delta->squash();
  _delta.reset();
}

} // namespace koinos::state_db
