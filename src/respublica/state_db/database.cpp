#include <respublica/state_db/database.hpp>
#include <respublica/state_db/delta_index.hpp>

namespace respublica::state_db {

database::database() noexcept:
    _index( std::make_shared< delta_index >() )
{}

database::~database() {}

void database::open( genesis_init_function init,
                     fork_resolution_algorithm algo,
                     const std::optional< std::filesystem::path >& p )
{
  _index->open( std::move( init ), algo, p );
}

void database::close()
{
  _index->close();
}

void database::reset()
{
  _index->reset();
}

permanent_state_node_ptr database::get( const state_node_id& node_id ) const
{
  if( auto delta = _index->get( node_id ); delta )
    return std::make_shared< permanent_state_node >( delta, _index );

  return permanent_state_node_ptr();
}

permanent_state_node_ptr database::root() const
{
  if( auto delta = _index->root(); delta )
    return std::make_shared< permanent_state_node >( delta, _index );

  return permanent_state_node_ptr();
}

} // namespace respublica::state_db
