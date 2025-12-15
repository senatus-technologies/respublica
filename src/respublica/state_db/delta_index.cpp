#include <respublica/state_db/delta_index.hpp>
#include <respublica/state_db/state_node.hpp>

namespace respublica::state_db {

state_delta_ptr fifo_comparator( const std::unordered_set< state_delta_ptr >&,
                                 const state_delta_ptr& head_block,
                                 const state_delta_ptr& )
{
  return head_block;
}

delta_index::~delta_index()
{
  close();
}

void delta_index::open( genesis_init_function init,
                        fork_resolution_algorithm algo,
                        const std::optional< std::filesystem::path >& path )
{
  state_node_comparator_function comp;

  switch( algo )
  {
    case fork_resolution_algorithm::fifo:
      [[fallthrough]];
    default:
      comp = &fifo_comparator;
  }

  open( std::move( init ), std::move( comp ), path );
}

void delta_index::open( genesis_init_function init,
                        state_node_comparator_function comp,
                        const std::optional< std::filesystem::path >& path )
{
  _path = path;
  _init = std::move( init );
  _comp = std::move( comp );

  auto root_delta = std::make_shared< state_delta >( _path );

  if( !root_delta->revision() )
  {
    std::shared_ptr< state_node > root = std::make_shared< temporary_state_node >( root_delta );
    _init( root );
  }

  root_delta->mark_complete();

  _index.insert( root_delta );
  _root = root_delta;
}

void delta_index::close()
{
  _index.clear();
  _root.reset();
}

void delta_index::reset()
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  _root->clear();
  open( _init, _comp, _path );
}

const state_delta_ptr& delta_index::root() const
{
  return _root;
}

state_delta_ptr delta_index::get( const state_node_id& id ) const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  if( auto itr = _index.find( id ); itr != _index.end() )
    return *itr;

  return state_delta_ptr();
}

void delta_index::add( const state_delta_ptr& ptr )
{
  if( !_index.insert( ptr ).second )
    throw std::runtime_error( "could not add state delta" );
}

void delta_index::mark_complete( const state_delta_ptr& ptr )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
}

void delta_index::remove( const state_delta_ptr& ptr, const std::unordered_set< state_node_id >& whitelist )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  if( ptr->id() == _root->id() )
    throw std::runtime_error( "cannot discard root node" );
}

void delta_index::commit( const state_delta_ptr& ptr )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  // If the node_id to commit is the root id, return. It is already committed.
  if( ptr->id() == _root->id() )
    return;

  auto old_root = _root;
  _root         = ptr;

  _index.modify( _index.find( ptr->id() ),
                 []( state_delta_ptr& n )
                 {
                   n->commit();
                 } );

  remove( old_root, { _root->id() } );
}

bool delta_index::is_open() const
{
  return (bool)_root;
}

} // namespace respublica::state_db
