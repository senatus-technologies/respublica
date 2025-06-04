#pragma once

#include <koinos/state_db/types.hpp>

#include <koinos/crypto/crypto.hpp>

#include <optional>
#include <span>

namespace koinos::state_db {

class state_node
{
public:
  state_node( std::shared_ptr< state_delta > delta );
  virtual ~state_node();

  /**
   * Fetch an object if one exists.
   */
  std::optional< std::span< const std::byte > > get( const object_space& space, std::span< const std::byte > key ) const;

  /**
   * Get the next object.
   */
  std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > > next( const object_space& space,
                                                                      std::span< const std::byte > key ) const;

  /**
   * Get the previous object.
   */
  std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > > previous( const object_space& space,
                                                                      std::span< const std::byte > key ) const;

  /**
   * Write an object into the state_node.
   */
  int64_t put( const object_space& space, std::span< const std::byte > key, std::span< const std::byte > value );

  /**
   * Remove an object from the state_node
   */
  int64_t remove( const object_space& space, std::span< const std::byte > key );

  /**
   * Returns a temporary child state node with this node as its parent.
   */
  std::shared_ptr< temporary_state_node > make_child() const;

  /**
   * Returns a temporary node with the same contents and parent as this node.
   */
  std::shared_ptr< temporary_state_node > clone() const;

  /**
   * Returns the state node id.
   */
  const state_node_id& id() const;

  /**
   * Returns the id of the parent state node.
   */
  const state_node_id& parent_id() const;

  /**
   * Returns the revision of the state node.
   */
  uint64_t revision() const;

protected:
  std::shared_ptr< state_delta > _delta;
};

class permanent_state_node : public state_node {
public:
  permanent_state_node( std::shared_ptr< state_delta > delta, std::shared_ptr< delta_index > index );
  ~permanent_state_node();

  /**
   * Returns if this node is final.
   */
  bool final() const;

  /**
   * Finalizes the node, preventing further writes to the node.
   */
  void finalize();

  /**
   * Returns the state node's merkle root. The node must be final.
   */
  const crypto::digest& merkle_root() const;

  /**
   * Discards the node from the node index.
   */
  void discard();

  /**
   * Commits this node to the root node, merging all nodes between this node
   * and the root node and making this node the new root node.
   */
  void commit();

  /**
   * Return a permanent child state node with this node as its parent.
   * Creating a permanent state node requires this node is finalized.
   */
  std::shared_ptr< permanent_state_node > make_child( const state_node_id& child_id ) const;

  /**
   * Clones this node and returns a new permanent state node with the
   * same contents and parent as this node.
   */
  std::shared_ptr< permanent_state_node > clone( const state_node_id& new_id ) const;

private:
  std::weak_ptr< delta_index > _index;
};

class temporary_state_node: public state_node
{
public:
  temporary_state_node( std::shared_ptr< state_delta > delta );
  ~temporary_state_node();

  /**
   * Squash the node in to the parent node. This call invalidates this state node.
   */
  void squash();
};

} // namespace koinos::state_db
