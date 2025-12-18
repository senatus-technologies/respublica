#include <respublica/memory/memory.hpp>
#include <respublica/state_db/state_delta.hpp>
#include <respublica/state_db/state_node.hpp>

namespace respublica::state_db {

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

std::int64_t state_node::remove( const object_space& space, std::span< const std::byte > key )
{
  return mutable_delta()->remove( make_compound_key( space, key ) );
}

std::shared_ptr< temporary_state_node > state_node::make_child()
{
  auto result = mutable_delta()->make_child();
  return std::make_shared< temporary_state_node >( result.child );
}

const state_node_id& state_node::id() const
{
  return delta()->id();
}

std::uint64_t state_node::revision() const
{
  return delta()->revision();
}

} // namespace respublica::state_db
