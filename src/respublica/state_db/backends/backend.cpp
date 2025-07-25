#include <respublica/state_db/backends/backend.hpp>

namespace respublica::state_db::backends {

abstract_backend::abstract_backend() {}

abstract_backend::abstract_backend( const state_node_id& id, std::uint64_t revision ):
    _id( id ),
    _revision( revision )
{}

bool abstract_backend::empty() const
{
  return size() == 0;
}

std::uint64_t abstract_backend::revision() const
{
  return _revision;
}

void abstract_backend::set_revision( std::uint64_t revision )
{
  _revision = revision;
}

const state_node_id& abstract_backend::id() const
{
  return _id;
}

void abstract_backend::set_id( const state_node_id& id )
{
  _id = id;
}

const digest& abstract_backend::merkle_root() const
{
  return _merkle_root;
}

void abstract_backend::set_merkle_root( const digest& merkle_root )
{
  _merkle_root = merkle_root;
}

} // namespace respublica::state_db::backends
