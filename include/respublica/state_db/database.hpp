#pragma once

#include <respublica/state_db/state_node.hpp>

#include <filesystem>
#include <vector>

namespace respublica::state_db {

/**
 * database is designed to provide parallel access to the database across
 * different states.
 *
 * It does by tracking positive state deltas, which can be merged on the fly
 * at read time to return the correct state of the database. A database
 * checkpoint is represented by the state_node class. Reads and writes happen
 * against a state_node.
 *
 * States are organized as a tree with the assumption that one path wins out
 * over time and cousin paths are discarded as the root is advanced.
 *
 * Currently, database is not thread safe. That is, calls directly on database
 * are not thread safe. (i.e. deleting a node concurrently to creating a new
 * node can leave database in an undefined state)
 *
 * Concurrency across state nodes is supported native to the implementation
 * without locks. Writes on a single state node need to be serialized, but
 * reads are implicitly parallel.
 *
 * TODO: Either extend the design of database to support concurrent access
 * or implement a some locking mechanism for access to the fork multi
 * index container.
 *
 * There is an additional corner case that is difficult to address.
 *
 * Upon squashing a state node, readers may be reading from the node that
 * is being squashed or an intermediate node between root and that node.
 * Relatively speaking, this should happen infrequently (on the order of once
 * per some number of seconds). As such, whatever guarantees concurrency
 * should heavily favor readers. Writing can happen lazily, preferably when
 * there is no contention from readers at all.
 */
class database final
{
public:
  database() noexcept;
  database( const database& ) = delete;
  database( database&& )      = delete;
  ~database();

  database& operator=( const database& ) = delete;
  database& operator=( database&& )      = delete;

  /**
   * Open the database.
   */
  void open( genesis_init_function init,
             fork_resolution_algorithm algo,
             const std::optional< std::filesystem::path >& path = {} );

  /**
   * Close the database.
   */
  void close();

  /**
   * Reset the database.
   */
  void reset();

  /**
   * Get an ancestor of a node at a particular revision
   */
  permanent_state_node_ptr at_revision( std::uint64_t revision, const state_node_id& child_id = null_id ) const;

  /**
   * Get the state_node for the given state_node_id.
   *
   * Return an empty pointer if no node for the given id exists.
   */
  permanent_state_node_ptr get( const state_node_id& node_id ) const;

  /**
   * Get and return the current "head" node.
   *
   * Head is determined by longest chain. Oldest
   * chain wins in a tie of length. Only finalized
   * nodes are eligible to become head.
   */
  permanent_state_node_ptr head() const;

  /**
   * Get and return a vector of all fork heads.
   *
   * Fork heads are any finalized nodes that do
   * not have finalized children.
   */
  std::vector< permanent_state_node_ptr > fork_heads() const;

  /**
   * Get and return the current "root" node.
   *
   * All state nodes are guaranteed to a descendant of root.
   */
  permanent_state_node_ptr root() const;

private:
  std::shared_ptr< delta_index > _index;
};

} // namespace respublica::state_db
