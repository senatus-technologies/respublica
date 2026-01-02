#include <respublica/state_db/delta_index.hpp>
#include <respublica/state_db/state_node.hpp>

#include <deque>

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

  auto impacted_nodes = ptr->mark_complete();

  for( const auto& node: impacted_nodes )
    update_node( node );

  // Incrementally update conflict cache for the newly completed node
  update_conflict_cache_for_node( ptr );
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

void delta_index::update_node( const state_delta_ptr& ptr )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  // Notify multi-index that node's indexed properties changed
  auto it = _index.find( ptr->id() );
  if( it != _index.end() )
  {
    _index.modify( it,
                   []( state_delta_ptr& )
                   {
                     // No-op lambda, just triggers reindexing
                   } );
  }
}

bool delta_index::is_open() const
{
  return (bool)_root;
}

std::unordered_set< state_delta_ptr >
delta_index::lock_and_filter_conflicts( const std::weak_ptr< state_delta >& node ) const
{
  std::unordered_set< state_delta_ptr > result;

  auto it = _conflict_cache.find( node );
  if( it == _conflict_cache.end() )
    return result;

  for( const auto& weak_conflict: it->second.conflicts_with )
  {
    if( auto locked = weak_conflict.lock() )
      result.insert( locked );
  }

  return result;
}

std::unordered_set< state_delta_ptr > delta_index::get_all_ancestors( const state_delta_ptr& node ) const
{
  std::unordered_set< state_delta_ptr > ancestors;
  std::deque< state_delta_ptr > queue;
  std::unordered_set< state_delta_ptr > visited;

  queue.push_back( node );
  visited.insert( node );

  while( !queue.empty() )
  {
    auto current = queue.front();
    queue.pop_front();

    for( const auto& parent: current->parents() )
    {
      if( visited.find( parent ) == visited.end() )
      {
        ancestors.insert( parent );
        queue.push_back( parent );
        visited.insert( parent );
      }
    }
  }

  return ancestors;
}

void delta_index::cache_conflict_if_exists( const state_delta_ptr& node1, const state_delta_ptr& node2 )
{
  if( node1->has_conflict( *node2 ) )
  {
    // Add bidirectional conflict entries
    std::weak_ptr< state_delta > weak1 = node1;
    std::weak_ptr< state_delta > weak2 = node2;

    _conflict_cache[ weak1 ].conflicts_with.insert( weak2 );
    _conflict_cache[ weak2 ].conflicts_with.insert( weak1 );
  }
}

void delta_index::update_conflict_cache_for_node( const state_delta_ptr& node )
{
  // Check the newly completed node against all other nodes in the index
  for( const auto& other_node: _index )
  {
    if( other_node != node )
    {
      cache_conflict_if_exists( node, other_node );
    }
  }
}

void delta_index::cleanup_conflict_cache()
{
  // Remove expired weak pointers
  for( auto it = _conflict_cache.begin(); it != _conflict_cache.end(); )
  {
    // Check if key is expired
    if( it->first.expired() )
    {
      it = _conflict_cache.erase( it );
    }
    else
    {
      // Clean up expired conflicts in the conflict set
      auto& conflicts = it->second.conflicts_with;
      for( auto conflict_it = conflicts.begin(); conflict_it != conflicts.end(); )
      {
        if( conflict_it->expired() )
        {
          conflict_it = conflicts.erase( conflict_it );
        }
        else
        {
          ++conflict_it;
        }
      }

      // Remove entry if conflict set is empty
      if( conflicts.empty() )
      {
        it = _conflict_cache.erase( it );
      }
      else
      {
        ++it;
      }
    }
  }
}

std::unordered_set< state_delta_ptr >
delta_index::get_conflict_closure( const state_delta_ptr& node,
                                   const std::unordered_set< state_delta_ptr >& candidate_pool ) const
{
  std::unordered_set< state_delta_ptr > closure;
  std::deque< state_delta_ptr > queue;

  closure.insert( node );
  queue.push_back( node );

  while( !queue.empty() )
  {
    auto current = queue.front();
    queue.pop_front();

    auto conflicts = lock_and_filter_conflicts( current );

    for( const auto& conflict: conflicts )
    {
      // Only include conflicts that are in the candidate pool
      if( candidate_pool.find( conflict ) != candidate_pool.end() && closure.find( conflict ) == closure.end() )
      {
        closure.insert( conflict );
        queue.push_back( conflict );
      }
    }
  }

  return closure;
}

std::optional< state_delta_ptr >
delta_index::resolve_conflict_set( const std::unordered_set< state_delta_ptr >& conflict_set,
                                   const std::unordered_set< state_delta_ptr >& current_edge_roots,
                                   const std::optional< protocol::account >& validator_account ) const
{
  // Base case: only one node in conflict set
  if( conflict_set.size() == 1 )
    return *conflict_set.begin();

  // Tier 1: Final nodes win
  std::unordered_set< state_delta_ptr > final_nodes;
  for( const auto& node: conflict_set )
  {
    if( node->final() )
      final_nodes.insert( node );
  }

  if( !final_nodes.empty() )
  {
    if( final_nodes.size() == 1 )
      return *final_nodes.begin();
    else
      // Recurse with filtered set
      return resolve_conflict_set( final_nodes, current_edge_roots, validator_account );
  }

  // Tier 2: Validator-approved nodes win (if validator_account provided)
  if( validator_account.has_value() )
  {
    std::unordered_set< state_delta_ptr > approved_nodes;
    for( const auto& node: conflict_set )
    {
      if( node->has_approval_from( *validator_account ) )
        approved_nodes.insert( node );
    }

    if( !approved_nodes.empty() )
    {
      if( approved_nodes.size() == 1 )
        return *approved_nodes.begin();
      else
        // Recurse without validator to avoid infinite loop
        return resolve_conflict_set( approved_nodes, current_edge_roots, std::nullopt );
    }
  }

  // Tier 3: Highest approval weight wins
  approval_weight_t max_approval = 0;
  for( const auto& node: conflict_set )
  {
    max_approval = std::max( max_approval, node->total_approval() );
  }

  std::unordered_set< state_delta_ptr > highest_approval_nodes;
  for( const auto& node: conflict_set )
  {
    if( node->total_approval() == max_approval )
      highest_approval_nodes.insert( node );
  }

  if( highest_approval_nodes.size() == 1 )
    return *highest_approval_nodes.begin();

  // Tier 4: FIFO (keep current edge set)
  for( const auto& node: conflict_set )
  {
    if( current_edge_roots.find( node ) != current_edge_roots.end() )
      return node;
  }

  // No node in conflict_set is in current edge set
  return std::nullopt;
}

std::vector< state_delta_ptr > delta_index::get_final_edges() const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  std::vector< state_delta_ptr > result;

  // Query the by_is_final_edge index for all nodes where is_final_edge() == true
  auto& final_edge_index = _index.get< by_is_final_edge >();
  auto range             = final_edge_index.equal_range( true );

  for( auto it = range.first; it != range.second; ++it )
  {
    result.push_back( *it );
  }

  return result;
}

std::vector< state_delta_ptr >
delta_index::get_edge_candidates( const std::optional< protocol::account >& validator_account ) const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  // Step 2: Query by_is_edge_candidate index
  std::vector< state_delta_ptr > candidates;
  auto& edge_candidate_index = _index.get< by_is_edge_candidate >();
  auto range                 = edge_candidate_index.equal_range( true );

  for( auto it = range.first; it != range.second; ++it )
  {
    candidates.push_back( *it );
  }

  // Step 3: Build ancestor map for each candidate
  std::unordered_map< state_delta_ptr, std::unordered_set< state_delta_ptr > > candidate_ancestors;
  for( const auto& candidate: candidates )
  {
    candidate_ancestors[ candidate ] = get_all_ancestors( candidate );
  }

  // Step 4: Find conflict root for each candidate
  std::unordered_map< state_delta_ptr, std::unordered_set< state_delta_ptr > > conflict_root_to_candidates;

  for( const auto& candidate: candidates )
  {
    // Find highest conflicting ancestor
    state_delta_ptr conflict_root = nullptr;

    // Walk ancestors from root toward candidate
    const auto& ancestors = candidate_ancestors[ candidate ];
    for( const auto& ancestor: ancestors )
    {
      std::weak_ptr< state_delta > weak_ancestor = ancestor;
      auto conflicts                             = lock_and_filter_conflicts( weak_ancestor );
      if( !conflicts.empty() )
      {
        // This ancestor has conflicts, keep it as potential conflict root
        // We want the highest (closest to candidate) conflicting ancestor
        if( !conflict_root || candidate_ancestors[ candidate ].count( conflict_root ) > 0 )
        {
          conflict_root = ancestor;
        }
      }
    }

    if( conflict_root )
    {
      conflict_root_to_candidates[ conflict_root ].insert( candidate );
    }
    else
    {
      // No conflicting ancestors, use candidate itself as root
      conflict_root_to_candidates[ candidate ].insert( candidate );
    }
  }

  // Step 5: Group conflict roots by transitive conflicts
  std::unordered_set< state_delta_ptr > visited_roots;
  std::vector< std::unordered_set< state_delta_ptr > > conflict_groups;

  // Build set of all roots for conflict closure queries
  std::unordered_set< state_delta_ptr > all_roots;
  for( const auto& [ root, _ ]: conflict_root_to_candidates )
  {
    all_roots.insert( root );
  }

  for( const auto& [ root, _ ]: conflict_root_to_candidates )
  {
    if( visited_roots.find( root ) != visited_roots.end() )
      continue;

    auto group = get_conflict_closure( root, all_roots );
    visited_roots.insert( group.begin(), group.end() );
    conflict_groups.push_back( group );
  }

  // Step 6: Resolve each conflict group and collect results
  std::vector< state_delta_ptr > result;

  for( const auto& group: conflict_groups )
  {
    if( group.size() == 1 )
    {
      // No conflicts in this group
      auto winner = *group.begin();
      for( const auto& candidate: conflict_root_to_candidates[ winner ] )
      {
        result.push_back( candidate );
      }
    }
    else
    {
      // Multiple conflicting roots, need to resolve
      std::unordered_set< state_delta_ptr > current_edge_roots;
      for( const auto& root: group )
      {
        current_edge_roots.insert( root );
      }

      auto maybe_winner = resolve_conflict_set( group, current_edge_roots, validator_account );

      if( maybe_winner.has_value() )
      {
        const auto& winner = maybe_winner.value();
        for( const auto& candidate: conflict_root_to_candidates[ winner ] )
        {
          result.push_back( candidate );
        }
      }
      else
      {
        // Tier 4: Keep all current edges (no clear winner)
        for( const auto& root: current_edge_roots )
        {
          if( conflict_root_to_candidates.find( root ) != conflict_root_to_candidates.end() )
          {
            for( const auto& candidate: conflict_root_to_candidates[ root ] )
            {
              result.push_back( candidate );
            }
          }
        }
      }
    }
  }

  return result;
}

} // namespace respublica::state_db
