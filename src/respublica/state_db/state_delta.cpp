#include <respublica/state_db/backends/map/map_backend.hpp>
#include <respublica/state_db/state_delta.hpp>

#include <algorithm>
#include <deque>

#include <respublica/crypto.hpp>

namespace respublica::state_db {

state_delta::state_delta() noexcept:
    state_delta( std::optional< std::filesystem::path >{} )
{}

state_delta::state_delta( const std::optional< std::filesystem::path >& p ) noexcept
{
#if 0
  if( p )
  {
    auto backend = std::make_shared< backends::rocksdb::rocksdb_backend >();
    backend->open( *p );
    _backend = backend;
  }
  else
#endif
  {
    _backend = std::make_shared< backends::map::map_backend >();
  }
}

std::int64_t state_delta::remove( std::vector< std::byte >&& key )
{
  if( complete() )
    throw std::runtime_error( "cannot modify a complete state delta" );

  std::int64_t size = 0;

  size += _backend->remove( key );

  if( !root() && size == 0 )
    if( auto current_value = get( key ); current_value )
      size -= std::ssize( key ) + std::ssize( *current_value );

  if( size )
    _removed_objects.emplace( std::move( key ) );

  return size;
}

std::optional< std::span< const std::byte > > state_delta::get( const std::vector< std::byte >& key ) const
{
  // Check own backend first (no tracking needed for own writes)
  if( removed( key ) )
    return {};

  if( auto value = _backend->get( key ); value )
    return value;

  // If we have parents, this is a read from parent state - track it
  if( !_complete && !_parents.empty() )
    _read_keys.insert( key );

  // Now search parents
  std::deque< std::shared_ptr< const state_delta > > visit_queue;
  std::set< std::shared_ptr< const state_delta > > visited;

  visit_queue.append_range( _parents );

  while( !visit_queue.empty() )
  {
    const auto& node = visit_queue.front();

    if( !visited.contains( node ) )
    {
      if( node->removed( key ) )
        return {};

      if( auto value = node->_backend->get( key ); value )
        return value;

      visit_queue.append_range( node->_parents );
      visited.insert( node );
    }

    visit_queue.pop_front();
  }

  return {};
}

void state_delta::squash()
{
  if( _parents.empty() )
    throw std::runtime_error( "cannot squash a state delta with no parents" );

  if( _parents.size() > 1 )
    throw std::runtime_error( "cannot squash a state delta with more than one parent" );

  if( root() )
    return;

  auto parent = _parents.at( 0 );

  // If an object is removed here and exists in the parent, it needs to only be removed in the parent
  // If an object is modified here, but removed in the parent, it needs to only be modified in the parent
  // These are O(m log n) operations. Because of this, squash should only be called from anonymous state
  // nodes, whose modifications are much smaller
  for( auto itr = _removed_objects.begin(); itr != _removed_objects.end(); itr = _removed_objects.begin() )
  {
    parent->_backend->remove( *itr );

    if( !parent->root() )
      parent->_removed_objects.insert( _removed_objects.extract( itr ) );
    else
      _removed_objects.erase( itr );
  }

  for( auto itr = _backend->begin(); itr != _backend->end(); itr = _backend->begin() )
  {
    if( !parent->root() )
      parent->_removed_objects.erase( itr->first );

    auto key_value_pair = itr.release();
    parent->_backend->put( std::move( key_value_pair.first ), std::move( key_value_pair.second ) );
  }
}

void state_delta::commit()
{
  /**
   * commit works in two distinct phases. The first is head recursion until we are at the root
   * delta. At the root, we grab the backend and begin a write batch that will encompass all
   * state writes and the final write of the metadata.
   *
   * The second phase is popping off the stack, writing state to the backend. After all deltas
   * have been written to the backend, we write metadata to the backend and end the write batch.
   *
   * The result is this delta becomes the new root delta and state is written to the root backend
   * atomically.
   */
  if( root() )
    throw std::runtime_error( "cannot commit root" );

  std::vector< std::shared_ptr< state_delta > > node_stack;
  std::set< std::shared_ptr< state_delta > > visited;

  node_stack.emplace_back( shared_from_this() );

  std::shared_ptr< state_delta > root;

  for( std::size_t i = 0; i < node_stack.size(); ++i )
  {
    auto& node = node_stack.at( i );

    for( auto& parent: node->_parents )
    {
      if( !parent->root() )
      {
        if( !visited.contains( parent ) )
        {
          node_stack.push_back( parent );
          visited.insert( parent );
        }
      }
      else
      {
        if( !root )
          root = parent;
      }
    }
  }

  if( !root )
    throw std::runtime_error( "node not connected to root" );

  auto& backend = root->_backend;
  backend->start_write_batch();

  while( !node_stack.empty() )
  {
    auto& node = node_stack.back();

    for( const auto& r_key: node->_removed_objects )
      backend->remove( r_key );

    for( auto itr = node->_backend->begin(); itr != node->_backend->end(); itr = node->_backend->begin() )
    {
      auto key_value_pair = itr.release();
      backend->put( std::move( key_value_pair.first ), std::move( key_value_pair.second ) );
    }

    node_stack.pop_back();
  }

  // Update metadata on the backend
  backend->set_revision( revision() );
  backend->set_id( id() );
  backend->set_merkle_root( merkle_root() );
  backend->store_metadata();

  // End the write batch making the entire merge atomic
  backend->end_write_batch();

  // Reset local variables to match new status as root delta
  _removed_objects.clear();
  _backend = backend;
  _parents.clear();
}

void state_delta::clear()
{
  _backend->clear();
  _removed_objects.clear();
}

bool state_delta::removed( const std::vector< std::byte >& key ) const
{
  return _removed_objects.find( key ) != _removed_objects.end();
}

bool state_delta::root() const
{
  return _parents.empty();
}

std::uint64_t state_delta::revision() const
{
  return _backend->revision();
}

void state_delta::set_revision( std::uint64_t revision )
{
  _backend->set_revision( revision );
}

bool state_delta::complete() const
{
  return _complete;
}

void state_delta::mark_complete()
{
  _complete = true;
}

const digest& state_delta::merkle_root() const
{
  if( !complete() )
    throw std::runtime_error( "cannot return merkle root of non-complete node" );

  if( !_merkle_root )
  {
    std::vector< std::span< const std::byte > > merkle_leafs;
    merkle_leafs.reserve( ( _backend->size() + _removed_objects.size() ) * 2 );

    // Both the backend and _removed_objects are sorted on key
    // We can do a merge between the two and have it be ordered
    auto backend_itr = _backend->begin();
    auto removed_itr = _removed_objects.begin();

    while( backend_itr != _backend->end() && removed_itr != _removed_objects.end() )
    {
      if( std::ranges::lexicographical_compare( backend_itr->first, *removed_itr ) )
      {
        merkle_leafs.emplace_back( backend_itr->first );
        merkle_leafs.emplace_back( backend_itr->second );
        ++backend_itr;
      }
      else
      {
        merkle_leafs.emplace_back( *removed_itr );
        merkle_leafs.emplace_back();
        ++removed_itr;
      }
    }

    while( backend_itr != _backend->end() )
    {
      merkle_leafs.emplace_back( backend_itr->first );
      merkle_leafs.emplace_back( backend_itr->second );
      ++backend_itr;
    }

    while( removed_itr != _removed_objects.end() )
    {
      merkle_leafs.emplace_back( *removed_itr );
      merkle_leafs.emplace_back();
      ++removed_itr;
    }

    _merkle_root = crypto::merkle_root( merkle_leafs );
  }

  return *_merkle_root;
}

result< std::shared_ptr< state_delta > > state_delta::create_delta(
  const state_node_id& id,
  std::vector< std::shared_ptr< state_delta > >&& parents,
  const protocol::account& creator,
  approval_weight_t creator_weight,
  approval_weight_t threshold )
{
  // Remove null parents first to avoid null dereference
  auto end_it = std::remove_if( parents.begin(),
                                parents.end(),
                                []( const std::shared_ptr< state_delta >& parent )
                                {
                                  return !parent;
                                } );
  parents.erase( end_it, parents.end() );

  // TODO: This conflict check may be removable as an optimization if the BFT protocol
  // guarantees that conflicting nodes cannot both reach the finalized threshold
  // Check no conflicting parents
  for( std::size_t i = 0; i < parents.size(); ++i )
  {
    for( std::size_t j = i + 1; j < parents.size(); ++j )
    {
      if( parents[ i ]->has_conflict( *parents[ j ] ) )
        return std::unexpected( make_error_code( state_db_errc::conflicting_parents ) );
    }
  }

  // Create node
  auto node = std::make_shared< state_delta >();

  // Set parents
  node->_parents = std::move( parents );

  // Initialize backend
  node->_backend = std::make_shared< backends::map::map_backend >();
  if( id != null_id )
    node->_backend->set_id( id );

  // Initialize approvals and threshold
  node->_approvals[ creator ] = creator_weight;
  node->_approval_threshold   = threshold;

  // Propagate approval to ancestors
  node->propagate_approval_to_ancestors( creator, creator_weight );

  return node;
}

std::shared_ptr< state_delta > state_delta::make_child( const state_node_id& id,
                                                        std::optional< protocol::account > creator,
                                                        approval_weight_t creator_weight,
                                                        approval_weight_t threshold )
{
  auto child      = std::make_shared< state_delta >();
  child->_parents = { shared_from_this() };
  child->_backend =
    std::make_shared< backends::map::map_backend >( ( id == null_id ) ? this->id() : id, revision() + 1 );

  if( creator )
  {
    // Initialize approvals and threshold
    child->_approvals[ *creator ] = creator_weight;
    child->_approval_threshold   = threshold;

    // Propagate approval to ancestors
    child->propagate_approval_to_ancestors( *creator, creator_weight );
  }

  return child;
}


const state_node_id& state_delta::id() const
{
  return _backend->id();
}

const std::vector< std::shared_ptr< state_delta > >& state_delta::parents() const
{
  return _parents;
}

bool state_delta::has_conflict( const state_delta& other ) const
{
  // First, find all ancestors of this node
  // TODO: We only need to check non-final ancestors as finalized ancestors cannot conflict
  std::set< std::shared_ptr< const state_delta > > this_ancestors;
  std::deque< std::shared_ptr< const state_delta > > queue;

  queue.emplace_back( shared_from_this() );

  while( !queue.empty() )
  {
    const auto& node = queue.front();

    if( !this_ancestors.contains( node ) )
    {
      this_ancestors.insert( node );
      queue.append_range( node->_parents );
    }

    queue.pop_front();
  }

  // Find all ancestors of other node
  std::set< std::shared_ptr< const state_delta > > other_ancestors;
  queue.clear();

  queue.emplace_back( other.shared_from_this() );

  while( !queue.empty() )
  {
    const auto& node = queue.front();

    if( !other_ancestors.contains( node ) )
    {
      other_ancestors.insert( node );
      queue.append_range( node->_parents );
    }

    queue.pop_front();
  }

  // Find common ancestors (intersection)
  std::set< std::shared_ptr< const state_delta > > common_ancestors;
  for( const auto& node: this_ancestors )
  {
    if( other_ancestors.contains( node ) )
      common_ancestors.insert( node );
  }

  // Now check for conflicts, starting from this node but stopping at common ancestors
  queue.clear();
  std::set< std::shared_ptr< const state_delta > > this_visited = common_ancestors;

  queue.emplace_back( shared_from_this() );

  while( !queue.empty() )
  {
    const auto& this_node = queue.front();

    if( !this_visited.contains( this_node ) )
    {
      // Check this_node against all nodes in other's ancestry (excluding common ancestors)
      std::deque< std::shared_ptr< const state_delta > > other_queue;
      std::set< std::shared_ptr< const state_delta > > other_visited = common_ancestors;

      other_queue.emplace_back( other.shared_from_this() );

      while( !other_queue.empty() )
      {
        const auto& other_node = other_queue.front();

        if( !other_visited.contains( other_node ) )
        {
          // Check write-write conflicts
          for( auto itr = this_node->_backend->begin(); itr != this_node->_backend->end(); ++itr )
          {
            if( other_node->_backend->get( itr->first ) || other_node->_removed_objects.contains( itr->first ) )
              return true;
          }

          for( const auto& removed_key: this_node->_removed_objects )
          {
            if( other_node->_backend->get( removed_key ) || other_node->_removed_objects.contains( removed_key ) )
              return true;
          }

          // Check read-after-write: this reads vs other writes
          for( const auto& read_key: this_node->_read_keys )
          {
            if( other_node->_backend->get( read_key ) || other_node->_removed_objects.contains( read_key ) )
              return true;
          }

          // Check read-after-write: other reads vs this writes
          for( const auto& other_read_key: other_node->_read_keys )
          {
            if( this_node->_backend->get( other_read_key ) || this_node->_removed_objects.contains( other_read_key ) )
              return true;
          }

          other_queue.append_range( other_node->_parents );
          other_visited.insert( other_node );
        }

        other_queue.pop_front();
      }

      queue.append_range( this_node->_parents );
      this_visited.insert( this_node );
    }

    queue.pop_front();
  }

  return false;
}

void state_delta::propagate_approval_to_ancestors( const protocol::account& approver, approval_weight_t weight )
{
  std::deque< std::shared_ptr< state_delta > > queue;
  std::set< std::shared_ptr< state_delta > > visited;

  queue.append_range( _parents );

  while( !queue.empty() )
  {
    auto ancestor = queue.front();
    queue.pop_front();

    if( !visited.contains( ancestor ) )
    {
      visited.insert( ancestor );

      // Only modify approvals if not already finalized
      if( !ancestor->_finalized )
      {
        // Add approval (union semantics - only count once)
        auto [ it, inserted ] = ancestor->_approvals.insert( { approver, weight } );
        if( !inserted && it->second != weight )
        {
          // Approver already exists - this shouldn't happen in normal operation
          // but handle gracefully by keeping existing weight
        }

        // Check if this ancestor newly meets threshold
        if( ancestor->total_approval() >= ancestor->_approval_threshold )
        {
          ancestor->finalize_grandparents_if_threshold_met();
        }

        // Continue propagation upward (only if not finalized, as finalized nodes have finalized parents)
        queue.append_range( ancestor->_parents );
      }
    }
  }
}

approval_weight_t state_delta::total_approval() const
{
  approval_weight_t total = 0;
  for( const auto& [ approver, weight ]: _approvals )
  {
    total += weight;
  }
  return total;
}

void state_delta::finalize_grandparents_if_threshold_met()
{
  // Walk the entire ancestry tree to finalize all grandparents
  std::deque< std::shared_ptr< state_delta > > queue;
  std::set< std::shared_ptr< state_delta > > visited;

  // Start from parents (one level up)
  queue.append_range( _parents );

  while( !queue.empty() )
  {
    auto node = queue.front();
    queue.pop_front();

    if( !visited.contains( node ) )
    {
      visited.insert( node );

      // Finalize all parents of this node (which are grandparents+ of this)
      for( auto& parent: node->_parents )
      {
        if( !parent->_finalized )
        {
          parent->_finalized = true;
        }
      }

      // Continue walking up the tree (only if not finalized, as finalized nodes have finalized parents)
      if( !node->_finalized )
      {
        queue.append_range( node->_parents );
      }
    }
  }
}

bool state_delta::finalized() const
{
  return _finalized;
}

} // namespace respublica::state_db
