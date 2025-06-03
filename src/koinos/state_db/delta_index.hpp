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
using fork_set                       = std::unordered_set< state_delta_ptr >;
using state_node_comparator_function = std::function< state_delta_ptr( const fork_set&, state_delta_ptr, state_delta_ptr ) >;

class delta_index {
private:
  struct by_id;
  struct by_revision;
  struct by_parent;

  using delta_multi_index_type = boost::multi_index_container<
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

public:
  delta_index();
  ~delta_index();

  void
  open( const std::optional< std::filesystem::path >& p, genesis_init_function init, fork_resolution_algorithm algo );
  void open( const std::optional< std::filesystem::path >& p,
             genesis_init_function init,
             state_node_comparator_function comp );
  void close();
  void reset();

  state_delta_ptr head() const;
  state_delta_ptr root() const;
  const fork_set& fork_heads() const;

  state_delta_ptr get_delta_at_revision( uint64_t revision, const state_node_id& child_id ) const;
  state_delta_ptr get_delta( const state_node_id& id ) const;
  void add_delta( state_delta_ptr ptr );
  void finalize_delta( state_delta_ptr ptr );
  void remove_delta( state_delta_ptr ptr, const std::unordered_set< state_node_id >& whitelist = {} );
  void commit_delta( state_delta_ptr );

  bool is_open() const;

private:
  std::optional< std::filesystem::path > _path;
  genesis_init_function _init          = nullptr;
  state_node_comparator_function _comp = nullptr;

  delta_multi_index_type _index;
  state_delta_ptr _root;
  state_delta_ptr _head;
  fork_set _fork_heads;
};

} // namespace koinos::state_db