#pragma once

#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/types.hpp>

#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <unordered_set>

namespace koinos::state_db {

using state_delta_ptr                = std::shared_ptr< state_delta >;
using state_node_comparator_function = std::function< state_delta_ptr( const std::unordered_set< state_delta_ptr >&, const state_delta_ptr&, const state_delta_ptr& ) >;

class delta_index {
private:
  struct by_id;
  struct by_parent;

  using delta_multi_index_type = boost::multi_index_container<
  state_delta_ptr,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag< by_id >,
      boost::multi_index::const_mem_fun< state_delta, const state_node_id&, &state_delta::id > >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag< by_parent >,
      boost::multi_index::const_mem_fun< state_delta, const state_node_id&, &state_delta::parent_id > > > >;

public:
  delta_index() noexcept = default;
  delta_index( const delta_index& ) = delete;
  delta_index( delta_index&& ) = delete;
  ~delta_index();

  delta_index& operator =( const delta_index& ) = delete;
  delta_index& operator =( delta_index&& ) = delete;

  void
  open( genesis_init_function init, fork_resolution_algorithm algo, const std::optional< std::filesystem::path >& path );
  void open( genesis_init_function init,
             state_node_comparator_function comp,
             const std::optional< std::filesystem::path >& path );
  void close();
  void reset();

  const state_delta_ptr& head() const;
  const state_delta_ptr& root() const;
  const std::unordered_set< state_delta_ptr >& fork_heads() const;

  state_delta_ptr get( const state_node_id& id ) const;
  state_delta_ptr at_revision( uint64_t revision, const state_node_id& child_id ) const;
  void add( const state_delta_ptr& ptr );
  void finalize( const state_delta_ptr& ptr );
  void remove( const state_delta_ptr& ptr, const std::unordered_set< state_node_id >& whitelist = {} );
  void commit( const state_delta_ptr& );

  bool is_open() const;

private:
  std::optional< std::filesystem::path > _path;
  genesis_init_function _init          = nullptr;
  state_node_comparator_function _comp = nullptr;

  delta_multi_index_type _index;
  state_delta_ptr _root;
  state_delta_ptr _head;
  std::unordered_set< state_delta_ptr > _fork_heads;
};

} // namespace koinos::state_db