
#include <koinos/state_db/state_db.hpp>
#include <koinos/state_db/state_delta.hpp>
#include <koinos/util/conversion.hpp>

#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <utility>

namespace koinos::state_db {

namespace detail {

struct by_id;
struct by_revision;
struct by_parent;

using state_delta_ptr = std::shared_ptr< state_delta >;

using state_multi_index_type = boost::multi_index_container<
  state_delta_ptr,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag< by_id >,
      boost::multi_index::const_mem_fun< state_delta, const state_node_id&, &state_delta::id > >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag< by_parent >,
      boost::multi_index::const_mem_fun< state_delta, const state_node_id&, &state_delta::parent_id > >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag< by_revision >,
      boost::multi_index::const_mem_fun< state_delta, uint64_t, &state_delta::revision > > > >;

inline std::vector< std::byte > make_compound_key( const object_space& space, key_type key )
{
  std::vector< std::byte > compound_key;
  compound_key.reserve( sizeof( space ) + key.size() );
  std::ranges::copy( key_type( reinterpret_cast< const std::byte* >( &space ), sizeof( space ) ),
                     std::back_inserter( compound_key ) );
  std::ranges::copy( key, std::back_inserter( compound_key ) );
  return compound_key;
}

/**
 * Private implementation of state_node interface.
 *
 * Maintains a pointer to database_impl,
 * only allows reads / writes if the node corresponds to the DB's current state.
 */
class state_node_impl final
{
public:
  state_node_impl() {}

  ~state_node_impl() {}

  std::optional< value_type > get_object( const object_space& space, key_type key ) const;
  int64_t put_object( const object_space& space, key_type key, value_type val );
  int64_t remove_object( const object_space& space, key_type key );
  const digest& merkle_root() const;

  state_delta_ptr _state;
};

/**
 * Private implementation of database interface.
 *
 * This class relies heavily on using chainbase as a backing store.
 * Only one state_node can be active at a time.  This is enforced by
 * _current_node which is a weak_ptr.  New nodes will throw
 * NodeNotExpired exception if a
 */
class database_impl final
{
public:
  database_impl() {}

  ~database_impl()
  {
    close();
  }

  void open( const std::optional< std::filesystem::path >& p,
             genesis_init_function init,
             fork_resolution_algorithm algo );
  void open( const std::optional< std::filesystem::path >& p,
             genesis_init_function init,
             state_node_comparator_function comp );
  void close();

  void reset();
  state_node_ptr
  get_node_at_revision( uint64_t revision, const state_node_id& child ) const;
  state_node_ptr get_node( const state_node_id& node_id ) const;
  state_node_ptr create_writable_node( const state_node_id& parent_id,
                                       const state_node_id& new_id,
                                       const protocol::block_header& header );
  state_node_ptr clone_node( const state_node_id& node_id,
                             const state_node_id& new_id,
                             const protocol::block_header& header );
  void finalize_node( const state_node_id& node );
  void discard_node( const state_node_id& node,
                     const std::unordered_set< state_node_id >& whitelist );
  void commit_node( const state_node_id& node );

  state_node_ptr get_head() const;
  std::vector< state_node_ptr > get_fork_heads() const;
  std::vector< state_node_ptr > get_all_nodes() const;
  state_node_ptr get_root() const;

  bool is_open() const;

  std::optional< std::filesystem::path > _path;
  genesis_init_function _init_func     = nullptr;
  state_node_comparator_function _comp = nullptr;

  state_multi_index_type _index;
  state_delta_ptr _head;
  std::map< state_node_id, state_delta_ptr > _fork_heads;
  state_delta_ptr _root;
};

void database_impl::reset()
{
  //
  // This method closes, wipes and re-opens the database.
  //
  // So the caller needs to be very careful to only call this method if deleting the database is desirable!
  //

  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  // Wipe and start over from empty database!
  _root->clear();
  open( _path, _init_func, _comp );
}

void database_impl::open( const std::optional< std::filesystem::path >& p,
                          genesis_init_function init,
                          fork_resolution_algorithm algo )
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

void database_impl::open( const std::optional< std::filesystem::path >& p,
                          genesis_init_function init,
                          state_node_comparator_function comp )
{
  auto root           = std::make_shared< state_node >();
  root->_impl->_state = std::make_shared< state_delta >( p );
  _init_func          = init;
  _comp               = comp;

  if( !root->revision() && root->_impl->_state->is_empty() && _init_func )
  {
    init( root );
  }
  root->_impl->_state->finalize();
  _index.insert( root->_impl->_state );
  _root = root->_impl->_state;
  _head = root->_impl->_state;
  _fork_heads.insert_or_assign( _head->id(), _head );

  _path = p;
}

void database_impl::close()
{
  _fork_heads.clear();
  _root.reset();
  _head.reset();
  _index.clear();
}

state_node_ptr database_impl::get_node_at_revision( uint64_t revision,
                                                    const state_node_id& child_id ) const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  if( revision < _root->revision() )
    throw std::runtime_error( "cannot ask for node with revision less than root." );

  if( revision == _root->revision() )
    return get_root();

  auto child = get_node( child_id );
  if( !child )
    child = get_head();

  state_delta_ptr delta = child->_impl->_state;

  while( delta->revision() > revision )
  {
    delta = delta->parent();
  }

  auto node_itr = _index.find( delta->id() );

  if( node_itr == _index.end() )
    throw std::runtime_error( "could not find state node associated with linked state_delta" );

  auto node           = std::make_shared< state_node >();
  node->_impl->_state = *node_itr;
  return node;
}

state_node_ptr database_impl::get_node( const state_node_id& node_id ) const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  auto node_itr = _index.find( node_id );

  if( node_itr != _index.end() )
  {
    auto node           = std::make_shared< state_node >();
    node->_impl->_state = *node_itr;
    return node;
  }

  return state_node_ptr();
}

state_node_ptr database_impl::create_writable_node( const state_node_id& parent_id,
                                                    const state_node_id& new_id,
                                                    const protocol::block_header& header )
{
  state_node_ptr parent_state = get_node( parent_id );

  if( !parent_state || !parent_state->is_finalized() )
    return state_node_ptr();

  auto node           = std::make_shared< state_node >();
  node->_impl->_state = parent_state->_impl->_state->make_child( new_id, header );

  return _index.insert( node->_impl->_state ).second ? node: state_node_ptr();
}

state_node_ptr database_impl::clone_node( const state_node_id& node_id,
                                          const state_node_id& new_id,
                                          const protocol::block_header& header )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  auto node = get_node( node_id );
  if( !node )
    throw std::runtime_error( "node not found" );
  if( node->is_finalized() )
    throw std::runtime_error( "cannot clone finalized node" );

  auto new_node           = std::make_shared< state_node >();
  new_node->_impl->_state = node->_impl->_state->clone( new_id, header );

  return _index.insert( new_node->_impl->_state ).second ? new_node : state_node_ptr();
}

void database_impl::finalize_node( const state_node_id& node_id )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  auto node = get_node( node_id );
  if( !node )
    throw std::runtime_error( "node not found" );

  node->_impl->_state->finalize();

  if( node->revision() > _head->revision() )
  {
    _head = node->_impl->_state;
  }
  else if( node->revision() == _head->revision() )
  {
    fork_list forks;
    forks.reserve( _fork_heads.size() );
    std::transform( std::begin( _fork_heads ),
                    std::end( _fork_heads ),
                    std::back_inserter( forks ),
                    []( const auto& entry )
                    {
                      state_node_ptr s = std::make_shared< state_node >();
                      s->_impl->_state = entry.second;
                      return s;
                    } );

    auto head = get_head();
    if( auto new_head = _comp( forks, head, node ); new_head != nullptr )
    {
      _head = new_head->_impl->_state;
    }
    else
    {
      _head         = head->parent()->_impl->_state;
      auto head_itr = _fork_heads.find( head->id() );
      if( head_itr != std::end( _fork_heads ) )
        _fork_heads.erase( head_itr );
      _fork_heads.insert_or_assign( head->parent()->id(), _head );
    }
  }

  // When node is finalized, parent node needs to be removed from heads, if it exists.
  if( node->parent_id() != _head->id() )
  {
    auto parent_itr = _fork_heads.find( node->parent_id() );
    if( parent_itr != std::end( _fork_heads ) )
      _fork_heads.erase( parent_itr );

    _fork_heads.insert_or_assign( node->id(), node->_impl->_state );
  }
}

void database_impl::discard_node( const state_node_id& node_id,
                                  const std::unordered_set< state_node_id >& whitelist )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  auto node = get_node( node_id );

  if( !node )
    return;

  if( node_id == _root->id() )
    throw std::runtime_error( "cannot discard root node" );

  std::vector< state_node_id > remove_queue{ node_id };
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
      {
        remove_queue.push_back( ( *previtr )->id() );
      }

      ++previtr;
    }
  }

  for( const auto& id: remove_queue )
  {
    auto itr = _index.find( id );
    if( itr != _index.end() )
      _index.erase( itr );

    // We may discard one or more fork heads when discarding a minority fork tree
    // For completeness, we'll check every node to see if it is a fork head
    auto fork_itr = _fork_heads.find( id );
    if( fork_itr != _fork_heads.end() )
      _fork_heads.erase( fork_itr );
  }

  // When node is discarded, if the parent node is not a parent of other nodes (no forks), add it to heads.
  auto fork_itr = previdx.find( node->parent_id() );
  if( fork_itr == previdx.end() )
  {
    auto parent_itr = _index.find( node->parent_id() );
    if( parent_itr == _index.end() )
      throw std::runtime_error( "discarded parent node not found in node index" );
    _fork_heads.insert_or_assign( ( *parent_itr )->id(), *parent_itr );
  }
}

void database_impl::commit_node( const state_node_id& node_id )
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );

  // If the node_id to commit is the root id, return. It is already committed.
  if( node_id == _root->id() )
    return;

  auto node = get_node( node_id );
  if( !node )
    throw std::runtime_error( "node not found" );

  auto old_root = _root;
  _root         = node->_impl->_state;

  _index.modify( _index.find( node_id ),
                 []( state_delta_ptr& n )
                 {
                   n->commit();
                 } );

  std::unordered_set< state_node_id > whitelist{ node_id };
  discard_node( old_root->id(), whitelist );
}

state_node_ptr database_impl::get_head() const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  auto head           = std::make_shared< state_node >();
  head->_impl->_state = _head;
  return head;
}

std::vector< state_node_ptr > database_impl::get_fork_heads() const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  std::vector< state_node_ptr > fork_heads;
  fork_heads.reserve( _fork_heads.size() );

  for( auto& head: _fork_heads )
  {
    auto fork_head           = std::make_shared< state_node >();
    fork_head->_impl->_state = head.second;
    fork_heads.push_back( fork_head );
  }

  return fork_heads;
}

std::vector< state_node_ptr > database_impl::get_all_nodes() const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  std::vector< state_node_ptr > nodes;
  nodes.reserve( _index.size() );

  for( const auto& delta: _index )
  {
    auto node           = std::make_shared< state_node >();
    node->_impl->_state = delta;
    nodes.push_back( node );
  }

  return nodes;
}

state_node_ptr database_impl::get_root() const
{
  if( !is_open() )
    throw std::runtime_error( "database is not open" );
  auto root           = std::make_shared< state_node >();
  root->_impl->_state = _root;
  return root;
}

bool database_impl::is_open() const
{
  return (bool)_root && (bool)_head;
}

std::optional< value_type > state_node_impl::get_object( const object_space& space, key_type key ) const
{
  return _state->find( make_compound_key( space, key ) );
}

int64_t state_node_impl::put_object( const object_space& space, key_type key, value_type value )
{
  if( _state->is_finalized() )
    throw std::runtime_error( "cannot write to a finalized node" );

  auto compound_key  = make_compound_key( space, key );
  int64_t bytes_used = 0;
  auto pobj          = _state->find( compound_key );

  if( pobj )
    bytes_used -= pobj->size();
  else
    bytes_used += compound_key.size();

  bytes_used += value.size();
  _state->put( std::move( compound_key ), value );

  return bytes_used;
}

int64_t state_node_impl::remove_object( const object_space& space, key_type key )
{
  if( _state->is_finalized() )
    throw std::runtime_error( "cannot write to a finalized node" );

  auto compound_key  = make_compound_key( space, key );
  int64_t bytes_used = 0;
  auto pobj          = _state->find( compound_key );

  if( pobj )
  {
    bytes_used -= pobj->size();
    bytes_used -= compound_key.size();
  }

  _state->erase( std::move( compound_key ) );

  return bytes_used;
}

const digest& state_node_impl::merkle_root() const
{
  return _state->merkle_root();
}

} // namespace detail

abstract_state_node::abstract_state_node():
    _impl( new detail::state_node_impl() )
{}

abstract_state_node::~abstract_state_node() {}

std::optional< value_type > abstract_state_node::get_object( const object_space& space, key_type key ) const
{
  return _impl->get_object( space, key );
}

std::optional< std::pair< key_type, value_type > > abstract_state_node::get_next_object( const object_space& space,
                                                                                         key_type key ) const
{
  return {};
}

std::optional< std::pair< key_type, value_type > > abstract_state_node::get_prev_object( const object_space& space,
                                                                                         key_type key ) const
{
  return {};
}

int64_t abstract_state_node::put_object( const object_space& space, key_type key, value_type val )
{
  return _impl->put_object( space, key, val );
}

int64_t abstract_state_node::remove_object( const object_space& space, key_type key )
{
  return _impl->remove_object( space, key );
}

bool abstract_state_node::is_finalized() const
{
  return _impl->_state->is_finalized();
}

const digest& abstract_state_node::merkle_root() const
{
  if( !is_finalized() )
    throw std::runtime_error( "node must be finalized to calculate merkle root" );

  return _impl->merkle_root();
}

anonymous_state_node_ptr abstract_state_node::create_anonymous_node()
{
  auto anonymous_node           = std::make_shared< anonymous_state_node >();
  anonymous_node->_parent       = shared_from_derived();
  anonymous_node->_impl->_state = _impl->_state->make_child();
  return anonymous_node;
}

state_node::state_node():
    abstract_state_node()
{}

state_node::~state_node() {}

const state_node_id& state_node::id() const
{
  return _impl->_state->id();
}

const state_node_id& state_node::parent_id() const
{
  return _impl->_state->parent_id();
}

uint64_t state_node::revision() const
{
  return _impl->_state->revision();
}

abstract_state_node_ptr state_node::parent() const
{
  auto parent_delta = _impl->_state->parent();
  if( parent_delta )
  {
    auto parent_node           = std::make_shared< state_node >();
    parent_node->_impl->_state = parent_delta;
    return parent_node;
  }

  return abstract_state_node_ptr();
}

const protocol::block_header& state_node::block_header() const
{
  return _impl->_state->block_header();
}

abstract_state_node_ptr state_node::shared_from_derived()
{
  return shared_from_this();
}

anonymous_state_node::anonymous_state_node():
    abstract_state_node()
{}

anonymous_state_node::anonymous_state_node::~anonymous_state_node() {}

const state_node_id& anonymous_state_node::id() const
{
  return _parent->id();
}

const state_node_id& anonymous_state_node::parent_id() const
{
  return _parent->parent_id();
}

uint64_t anonymous_state_node::revision() const
{
  return _parent->revision();
}

abstract_state_node_ptr anonymous_state_node::parent() const
{
  return _parent;
}

const protocol::block_header& anonymous_state_node::block_header() const
{
  return _parent->block_header();
}

void anonymous_state_node::commit()
{
  if( _parent->is_finalized() )
    throw std::runtime_error( "cannot commit to a finalized node" );
  _impl->_state->squash();
  reset();
}

void anonymous_state_node::reset()
{
  _impl->_state = _impl->_state->make_child();
}

abstract_state_node_ptr anonymous_state_node::shared_from_derived()
{
  return shared_from_this();
}

state_node_ptr fifo_comparator( fork_list& forks, state_node_ptr current_head, state_node_ptr new_head )
{
  return current_head;
}

state_node_ptr block_time_comparator( fork_list& forks, state_node_ptr head_block, state_node_ptr new_block )
{
  return new_block->block_header().timestamp < head_block->block_header().timestamp ? new_block : head_block;
}

state_node_ptr pob_comparator( fork_list& forks, state_node_ptr head_block, state_node_ptr new_block )
{
  if( head_block->block_header().signer != new_block->block_header().signer )
    return new_block->block_header().timestamp < head_block->block_header().timestamp ? new_block : head_block;

  auto it = std::find_if( std::begin( forks ),
                          std::end( forks ),
                          [ & ]( state_node_ptr p )
                          {
                            return p->id() == head_block->id();
                          } );
  if( it != std::end( forks ) )
    forks.erase( it );

  struct
  {
    bool operator()( abstract_state_node_ptr a, abstract_state_node_ptr b ) const
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
    std::sort( std::begin( forks ), std::end( forks ), priority_algorithm );
    it = std::begin( forks );
    return priority_algorithm( head_block->parent(), *it ) ? state_node_ptr() : *it;
  }

  return state_node_ptr();
}

database::database():
    impl( new detail::database_impl() )
{}

database::~database() {}

void database::open( const std::optional< std::filesystem::path >& p,
                     genesis_init_function init,
                     fork_resolution_algorithm algo )
{
  impl->open( p, init, algo );
}

void database::open( const std::optional< std::filesystem::path >& p,
                     genesis_init_function init,
                     state_node_comparator_function comp )
{
  impl->open( p, init, comp );
}

void database::close()
{
  impl->close();
}

void database::reset()
{
  impl->reset();
}

state_node_ptr
database::get_node_at_revision( uint64_t revision, const state_node_id& child_id ) const
{
  return impl->get_node_at_revision( revision, child_id );
}

state_node_ptr database::get_node( const state_node_id& node_id ) const
{
  return impl->get_node( node_id );
}

state_node_ptr database::create_writable_node( const state_node_id& parent_id,
                                               const state_node_id& new_id,
                                               const protocol::block_header& header )
{
  return impl->create_writable_node( parent_id, new_id, header );
}


state_node_ptr database::clone_node( const state_node_id& node_id,
                                     const state_node_id& new_id,
                                     const protocol::block_header& header )
{
  return impl->clone_node( node_id, new_id, header );
}

void database::finalize_node( const state_node_id& node_id )
{
  impl->finalize_node( node_id );
}

void database::discard_node( const state_node_id& node_id )
{
  static const std::unordered_set< state_node_id > whitelist;
  impl->discard_node( node_id, whitelist );
}

void database::commit_node( const state_node_id& node_id )
{
  impl->commit_node( node_id );
}

state_node_ptr database::get_head() const
{
  return impl->get_head();
}

std::vector< state_node_ptr > database::get_fork_heads() const
{
  return impl->get_fork_heads();
}

std::vector< state_node_ptr > database::get_all_nodes() const
{
  return impl->get_all_nodes();
}

state_node_ptr database::get_root() const
{
  return impl->get_root();
}

} // namespace koinos::state_db
