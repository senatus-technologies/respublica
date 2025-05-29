#include <koinos/state_db/state_delta.hpp>

#include <koinos/crypto/crypto.hpp>

namespace koinos::state_db::detail {

state_delta::state_delta( const std::optional< std::filesystem::path >& p )
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

  _revision    = _backend->revision();
  _id          = _backend->id();
  _merkle_root = _backend->merkle_root();
}

void state_delta::put( key_type k, value_type v )
{
  _backend->put( k, v );
}

void state_delta::erase( key_type k )
{
  if( find( k ) )
  {
    _backend->erase( k );
    _removed_objects.emplace_back( k.begin(), k.end() );
    _span_map.emplace( key_type( k ), --_removed_objects.end() );
  }
}

std::optional< value_type > state_delta::find( key_type key ) const
{
  if( auto value = _backend->get( key ); value )
    return value;

  if( is_root() || is_removed( key ) )
    return {};

  return _parent->find( key );
}

void state_delta::squash()
{
  if( is_root() )
    return;

  // If an object is removed here and exists in the parent, it needs to only be removed in the parent
  // If an object is modified here, but removed in the parent, it needs to only be modified in the parent
  // These are O(m log n) operations. Because of this, squash should only be called from anonymouse state
  // nodes, whose modifications are much smaller
  for( const auto& r_key: _removed_objects )
  {
    _parent->_backend->erase( key_type( r_key ) );

    if( !_parent->is_root() )
    {
      _parent->_removed_objects.emplace_back( r_key );
      _parent->_span_map.insert_or_assign( r_key, --_parent->_removed_objects.end() );
    }
  }

  for( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
  {
    _parent->_backend->put( itr.key(), *itr );

    if( !_parent->is_root() )
    {
      if( auto span_itr = _parent->_span_map.find( itr.key() ); span_itr != _parent->_span_map.end() )
      {
        _parent->_removed_objects.erase( span_itr->second );
        _parent->_span_map.erase( span_itr );
      }
    }
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
  if( is_root() )
    throw std::runtime_error( "cannot commit root" );

  std::vector< std::shared_ptr< state_delta > > node_stack;
  auto current_node = shared_from_this();

  while( current_node )
  {
    node_stack.push_back( current_node );
    current_node = current_node->_parent;
  }

  // Because we already asserted we were not root, there will always exist a minimum of two nodes in the stack,
  // this and root.
  auto backend = node_stack.back()->_backend;
  node_stack.back()->_backend.reset();
  node_stack.pop_back();

  // Start the write batch
  backend->start_write_batch();

  // While there are nodes on the stack, write them to the backend
  while( node_stack.size() )
  {
    auto& node = node_stack.back();

    for( const auto& r_key: node->_span_map )
      backend->erase( r_key.first );

    for( auto itr = node->_backend->begin(); itr != node->_backend->end(); ++itr )
      backend->put( itr.key(), *itr );

    node_stack.pop_back();
  }

  // Update metadata on the backend
  backend->set_block_header( block_header() );
  backend->set_revision( _revision );
  backend->set_id( _id );
  backend->set_merkle_root( merkle_root() );
  backend->store_metadata();

  // End the write batch making the entire merge atomic
  backend->end_write_batch();

  // Reset local variables to match new status as root delta
  _span_map.clear();
  _removed_objects.clear();
  _backend = backend;
  _parent.reset();
}

void state_delta::clear()
{
  _backend->clear();
  _span_map.clear();
  _removed_objects.clear();

  _revision = 0;
  std::fill( std::begin( _id ), std::end( _id ), std::byte{ 0x00 } );
}

bool state_delta::is_modified( key_type k ) const
{
  return _backend->get( k ) || _span_map.find( k ) != _span_map.end();
}

bool state_delta::is_removed( key_type k ) const
{
  return _span_map.find( k ) != _span_map.end();
}

bool state_delta::is_root() const
{
  return !_parent;
}

uint64_t state_delta::revision() const
{
  return _revision;
}

void state_delta::set_revision( uint64_t revision )
{
  _revision = revision;

  if( is_root() )
    _backend->set_revision( revision );
}

bool state_delta::is_finalized() const
{
  return _finalized;
}

void state_delta::finalize()
{
  _finalized = true;
}

std::condition_variable_any& state_delta::cv()
{
  return _cv;
}

std::timed_mutex& state_delta::cv_mutex()
{
  return _cv_mutex;
}

const digest& state_delta::merkle_root() const
{
  if( !_merkle_root )
  {
    std::vector< key_type > object_keys;
    object_keys.reserve( _backend->size() + _span_map.size() );

    for( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
      object_keys.push_back( itr.key() );

    for( const auto& removed: _span_map )
      object_keys.push_back( removed.first );

    std::sort( object_keys.begin(), object_keys.end(), bytes_less{} );

    std::vector< crypto::multihash > merkle_leafs;
    merkle_leafs.reserve( object_keys.size() * 2 );

    for( const auto& key: object_keys )
    {
      // if( auto hash = crypto::hash( crypto::multicodec::sha2_256, key ); hash )
      //   merkle_leafs.emplace_back( std::move( *hash ) );
      // else
      //   throw std::runtime_error( std::string( hash.error().message() ) );

      // auto value = _backend->get( key );
      // if( auto hash = crypto::hash( crypto::multicodec::sha2_256, value ? *value : value_type() ); hash )
      //   merkle_leafs.emplace_back( std::move( *hash ) );
      // else
      //   throw std::runtime_error( std::string( hash.error().message() ) );
    }

    if( auto tree = crypto::merkle_tree< crypto::multihash >::create( crypto::multicodec::sha2_256, merkle_leafs );
        tree )
    {
      auto digest  = tree->root()->hash().digest();
      _merkle_root = std::array< std::byte, 32 >();
      std::copy( digest.begin(), digest.end(), _merkle_root->begin() );
    }
    else
      throw std::runtime_error( std::string( tree.error().message() ) );
  }

  return *_merkle_root;
}

const protocol::block_header& state_delta::block_header() const
{
  return _backend->block_header();
}

std::shared_ptr< state_delta > state_delta::make_child( const state_node_id& id, const protocol::block_header& header )
{
  auto child       = std::make_shared< state_delta >();
  child->_parent   = shared_from_this();
  child->_id       = id;
  child->_revision = _revision + 1;
  child->_backend  = std::make_shared< backends::map::map_backend >( child->_revision, child->_id, header );
  child->_backend->set_block_header( header );

  return child;
}

std::shared_ptr< state_delta > state_delta::clone( const state_node_id& id, const protocol::block_header& header )
{
  auto new_node              = std::make_shared< state_delta >();
  new_node->_parent          = _parent;
  new_node->_backend         = _backend->clone();
  new_node->_removed_objects = _removed_objects;

  for( auto itr = new_node->_removed_objects.begin(); itr != new_node->_removed_objects.end(); ++itr )
    new_node->_span_map.insert_or_assign( key_type( *itr ), itr );

  new_node->_id          = id;
  new_node->_revision    = _revision;
  new_node->_merkle_root = _merkle_root;

  new_node->_finalized = _finalized;

  new_node->_backend->set_id( id );
  new_node->_backend->set_revision( _revision );
  new_node->_backend->set_block_header( header );

  if( _merkle_root )
  {
    new_node->_backend->set_merkle_root( *_merkle_root );
  }

  return new_node;
}

const std::shared_ptr< backends::abstract_backend > state_delta::backend() const
{
  return _backend;
}

const state_node_id& state_delta::id() const
{
  return _id;
}

const state_node_id& state_delta::parent_id() const
{
  return _parent ? _parent->_id : null_id;
}

std::shared_ptr< state_delta > state_delta::parent() const
{
  return _parent;
}

bool state_delta::is_empty() const
{
  if( _backend->size() )
    return false;
  else if( _parent )
    return _parent->is_empty();

  return true;
}

std::shared_ptr< state_delta > state_delta::get_root()
{
  if( !is_root() )
  {
    if( _parent->is_root() )
      return _parent;
    else
      return _parent->get_root();
  }

  return std::shared_ptr< state_delta >();
}

} // namespace koinos::state_db::detail
