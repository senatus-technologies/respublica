#include <koinos/state_db/delta_index.hpp>
#include <koinos/state_db/state_node.hpp>

namespace koinos::state_db {

state_delta_ptr fifo_comparator( const fork_set&, state_delta_ptr head_block, state_delta_ptr )
{
  return head_block;
}

state_delta_ptr block_time_comparator( const fork_set&, state_delta_ptr head_block, state_delta_ptr new_block )
{
  return new_block->block_header().timestamp < head_block->block_header().timestamp ? new_block : head_block;
}

state_delta_ptr pob_comparator( const fork_set& forks, state_delta_ptr head_block, state_delta_ptr new_block )
{
  if( head_block->block_header().signer != new_block->block_header().signer )
    return new_block->block_header().timestamp < head_block->block_header().timestamp ? new_block : head_block;

  struct
  {
    bool operator()( state_delta_ptr a, state_delta_ptr b ) const
    {
      if( a->revision() > b->revision() )
        return true;
      else if( a->revision() < b->revision() )
        return false;

      if( a->block_header().timestamp < b->block_header().timestamp )
        return true;
      else if( a->block_header().timestamp > b->block_header().timestamp )
        return false;

      if( a->id() < b->id() )
        return true;

      return false;
    }
  } priority_algorithm;

  if( std::size( forks ) )
  {
    std::vector< state_delta_ptr > fork_list;
    fork_list.reserve( forks.size() );
    std::ranges::copy( forks, std::back_inserter( fork_list ) );
    std::sort( std::begin( fork_list ), std::end( fork_list ), priority_algorithm );
    auto it = std::begin( fork_list );
    return priority_algorithm( head_block->parent(), *it ) ? state_delta_ptr() : *it;
  }

  return state_delta_ptr();
}

delta_index::delta_index()
{}

delta_index::~delta_index()
{
  close();
}

void
delta_index::open( const std::optional< std::filesystem::path >& p, genesis_init_function init, fork_resolution_algorithm algo )
{
  state_node_comparator_function comp;

  switch( algo )
  {
    case fork_resolution_algorithm::block_time:
      comp = &block_time_comparator;
      break;
    case fork_resolution_algorithm::pob:
      comp = &pob_comparator;
      break;
    case fork_resolution_algorithm::fifo:
      [[fallthrough]];
    default:
      comp = &fifo_comparator;
  }

  open( p, init, comp );
}

void delta_index::open( const std::optional< std::filesystem::path >& p,
            genesis_init_function init,
            state_node_comparator_function comp )
{
  _path = p;
  _init = init;
  _comp = comp;

  auto root_delta = std::make_shared< state_delta >( _path );

  if( !root_delta->revision() )
    _init( std::make_shared< state_node >( root_delta ) );

  root_delta->finalize();

  _index.insert( root_delta );
  _fork_heads.insert( root_delta );
  _root = root_delta;
  _head = root_delta;
}

void delta_index::close()
{
  _index.clear();
  _fork_heads.clear();
  _root.reset();
  _head.reset();
}

void delta_index::reset()
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  _root->clear();
  open( _path, _init, _comp );
}

state_delta_ptr delta_index::root() const
{
  return _root;
}

state_delta_ptr delta_index::head() const
{
  return _head;
}

const std::unordered_set< state_delta_ptr >& delta_index::fork_heads() const
{
  return _fork_heads;
}

state_delta_ptr delta_index::get_delta_at_revision( uint64_t revision, const state_node_id& child_id ) const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  if( revision < _root->revision() )
    throw std::runtime_error( "cannot ask for node with revision less than root." );

  if( revision == _root->revision() )
    return root();

  auto delta = get_delta( child_id );
  if( !delta )
    delta = head();

  while( delta->revision() > revision )
    delta = delta->parent();

  return delta;
}

state_delta_ptr delta_index::get_delta( const state_node_id& id ) const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  if( auto itr = _index.find( id ); itr != _index.end() )
    return *itr;

  return state_delta_ptr();
}

void delta_index::add_delta( state_delta_ptr ptr )
{
  if( !_index.insert( ptr ).second )
    throw std::runtime_error( "could not add state delta" );
}

void delta_index::finalize_delta( state_delta_ptr ptr )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  if( ptr->revision() > _head->revision() )
    _head = ptr;
  else if( ptr->revision() == _head->revision() )
  {
    if( auto new_head = _comp( _fork_heads, _head, ptr ); new_head )
      _head = new_head;
    else
    {
      // For some reason the current head is no longer head, then its parent should become head
      _fork_heads.erase( _head );
      _head = _head->parent();
    }
  }

  // When node is finalized, it's parent node needs to be removed from fork heads heads, if it exists.
  _fork_heads.erase( ptr->parent() );
  _fork_heads.insert( _head );
}

void delta_index::remove_delta( state_delta_ptr ptr, const std::unordered_set< state_node_id >& whitelist )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  if( ptr->id() == _root->id() )
    throw std::runtime_error( "cannot discard root node" );

  std::vector< state_node_id > remove_queue{ ptr->id() };
  const auto& previdx = _index.template get< by_parent >();
  const auto head_id  = _head->id();

  for( uint32_t i = 0; i < remove_queue.size(); ++i )
  {
    if( remove_queue[ i ] == head_id )
      throw std::runtime_error( "cannot discard an ancestor of head" );

    auto previtr = previdx.lower_bound( remove_queue[ i ] );
    while( previtr != previdx.end() && ( *previtr )->parent_id() == remove_queue[ i ] )
    {
      // Do not remove nodes on the whitelist
      if( whitelist.find( ( *previtr )->id() ) == whitelist.end() )
        remove_queue.push_back( ( *previtr )->id() );

      ++previtr;
    }
  }

  for( const auto& id: remove_queue )
  {
    if( auto itr = _index.find( id ); itr != _index.end() )
    {
      _index.erase( itr );

      // We may discard one or more fork heads when discarding a minority fork tree
      // For completeness, we'll check every node to see if it is a fork head
      _fork_heads.erase( *itr );
    }
  }

  // When node is discarded, if the parent node is not a parent of other nodes (no forks), add it to heads.
  _fork_heads.erase( ptr->parent() );
}

void delta_index::commit_delta( state_delta_ptr ptr )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  // If the node_id to commit is the root id, return. It is already committed.
  if( ptr->id() == _root->id() )
    return;

  auto old_root = _root;
  _root = ptr;

  _index.modify( _index.find( ptr->id() ),
                 []( state_delta_ptr& n )
                 {
                   n->commit();
                 } );

  remove_delta( old_root, { _root->id() } );
}

bool delta_index::is_open() const
{
  return (bool)_root && (bool)_head;
}

} // namespace koinos::state_db