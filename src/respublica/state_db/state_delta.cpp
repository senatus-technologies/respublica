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

state_delta::state_delta( const std::vector< std::shared_ptr< state_delta > >& parents ) noexcept:
    _parents( parents ),
    _backend( std::make_shared< backends::map::map_backend >() )
{
  [[maybe_unused]]
  auto _ = std::remove_if( _parents.begin(), _parents.end(), []( const std::shared_ptr< state_delta >& parent ){
    return !parent;
  });
}

state_delta::state_delta( std::vector< std::shared_ptr< state_delta > >&& parents ) noexcept:
    _parents( std::move( parents ) ),
    _backend( std::make_shared< backends::map::map_backend >() )
{
  [[maybe_unused]]
  auto _ = std::remove_if( _parents.begin(), _parents.end(), []( const std::shared_ptr< state_delta >& parent ){
    return !parent;
  });
}

std::int64_t state_delta::remove( std::vector< std::byte >&& key )
{
  if( final() )
    throw std::runtime_error( "cannot modify a final state delta" );

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
  std::deque< std::shared_ptr< const state_delta > > visit_queue;
  std::set< std::shared_ptr< const state_delta > > visited;

  visit_queue.emplace_back( shared_from_this() );

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

    for( auto& parent : node->_parents )
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

bool state_delta::final() const
{
  return _final;
}

void state_delta::finalize()
{
  _final = true;
}

const digest& state_delta::merkle_root() const
{
  if( !final() )
    throw std::runtime_error( "cannot return merkle root of non-final node" );

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

std::shared_ptr< state_delta > state_delta::make_child( const state_node_id& id )
{
  auto child     = std::make_shared< state_delta >();
  child->_parents = { shared_from_this() };
  child->_backend =
    std::make_shared< backends::map::map_backend >( ( id == null_id ) ? this->id() : id, revision() + 1 );

  return child;
}

std::shared_ptr< state_delta > state_delta::clone( const state_node_id& id ) const
{
  auto new_node              = std::make_shared< state_delta >();
  new_node->_parents         = _parents;
  new_node->_removed_objects = _removed_objects;
  new_node->_final           = _final;
  new_node->_merkle_root     = _merkle_root;
  new_node->_backend         = _backend->clone();

  if( id != null_id )
    new_node->_backend->set_id( id );

  if( _merkle_root )
    new_node->_backend->set_merkle_root( *_merkle_root );

  return new_node;
}

const state_node_id& state_delta::id() const
{
  return _backend->id();
}

const std::vector< std::shared_ptr< state_delta > >& state_delta::parents() const
{
  return _parents;
}

} // namespace respublica::state_db
