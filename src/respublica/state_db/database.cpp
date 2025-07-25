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

permanent_state_node_ptr database::at_revision( std::uint64_t revision, const state_node_id& child_id ) const
{
  if( auto delta = _index->at_revision( revision, child_id ); delta )
    return std::make_shared< permanent_state_node >( delta, _index );

  return permanent_state_node_ptr();
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

permanent_state_node_ptr database::head() const
{
  if( auto delta = _index->head(); delta )
    return std::make_shared< permanent_state_node >( delta, _index );

  return permanent_state_node_ptr();
}

std::vector< permanent_state_node_ptr > database::fork_heads() const
{
  auto fork_deltas = _index->fork_heads();
  std::vector< permanent_state_node_ptr > fork_heads;
  fork_heads.reserve( fork_deltas.size() );

  for( const auto& delta: fork_deltas )
    fork_heads.emplace_back( std::make_shared< permanent_state_node >( delta, _index ) );

  return fork_heads;
}

} // namespace respublica::state_db
