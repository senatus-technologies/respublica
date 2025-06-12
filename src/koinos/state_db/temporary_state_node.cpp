#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/state_node.hpp>

namespace koinos::state_db {

temporary_state_node::temporary_state_node( const std::shared_ptr< state_delta >& delta ) noexcept:
    _delta( delta )
{}

std::shared_ptr< state_delta > temporary_state_node::mutable_delta()
{
  return _delta;
}

const std::shared_ptr< state_delta >& temporary_state_node::delta() const
{
  return _delta;
}

void temporary_state_node::squash()
{
  if( !_delta )
    throw std::runtime_error( "state node has already been squashed" );

  if( _delta->parent()->final() )
    throw std::runtime_error( "parent state node is final" );

  _delta->squash();
  _delta.reset();
}

} // namespace koinos::state_db
