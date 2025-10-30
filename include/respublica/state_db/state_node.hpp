#pragma once

#include <respublica/memory.hpp>
#include <respublica/state_db/state_delta.hpp>
#include <respublica/state_db/types.hpp>

#include <optional>
#include <ranges>
#include <span>

namespace respublica::state_db {

inline std::vector< std::byte > make_compound_key( const object_space& space, std::span< const std::byte > key )
{
  std::vector< std::byte > compound_key;
  compound_key.reserve( sizeof( space ) + key.size() );
  std::ranges::copy( memory::as_bytes( space ), std::back_inserter( compound_key ) );
  std::ranges::copy( key, std::back_inserter( compound_key ) );
  return compound_key;
}

class state_node
{
public:
  state_node() noexcept                = default;
  state_node( const state_node& node ) = delete;
  state_node( state_node&& node )      = delete;
  virtual ~state_node()                = default;

  state_node& operator=( const state_node& node ) = delete;
  state_node& operator=( state_node&& node )      = delete;

  /**
   * Fetch an object if one exists.
   */
  std::optional< std::span< const std::byte > > get( const object_space& space,
                                                     std::span< const std::byte > key ) const;

  /**
   * Get the next object.
   */
  std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > >
  next( const object_space& space, std::span< const std::byte > key ) const;

  /**
   * Get the previous object.
   */
  std::optional< std::pair< std::span< const std::byte >, std::span< const std::byte > > >
  previous( const object_space& space, std::span< const std::byte > key ) const;

  /**
   * Write an object into the state_node.
   */
  template< std::ranges::range ValueType >
  std::int64_t put( const object_space& space, std::span< const std::byte > key, const ValueType& value )
  {
    return delta()->put( make_compound_key( space, key ), value );
  }

  /**
   * Remove an object from the state_node
   */
  std::int64_t remove( const object_space& space, std::span< const std::byte > key );

  /**
   * Returns a temporary child state node with this node as its parent.
   */
  std::shared_ptr< temporary_state_node > make_child();

  /**
   * Returns a temporary node with the same contents and parent as this node.
   */
  std::shared_ptr< temporary_state_node > clone() const;

  /**
   * Returns the state node id.
   */
  const state_node_id& id() const;

  /**
   * Returns the revision of the state node.
   */
  std::uint64_t revision() const;

private:
  virtual std::shared_ptr< state_delta > mutable_delta()      = 0;
  virtual const std::shared_ptr< state_delta >& delta() const = 0;
};

class permanent_state_node final: public state_node
{
public:
  permanent_state_node( const std::shared_ptr< state_delta >& delta,
                        const std::shared_ptr< delta_index >& index ) noexcept;
  permanent_state_node( const permanent_state_node& node ) = delete;
  permanent_state_node( permanent_state_node&& node )      = delete;
  ~permanent_state_node() override                         = default;

  permanent_state_node operator=( const permanent_state_node& node ) = delete;
  permanent_state_node operator=( permanent_state_node&& node )      = delete;

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
  const digest& merkle_root() const;

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
  std::shared_ptr< state_delta > mutable_delta() override;
  const std::shared_ptr< state_delta >& delta() const override;

  std::shared_ptr< state_delta > _delta;
  std::weak_ptr< delta_index > _index;
};

class temporary_state_node final: public state_node
{
public:
  temporary_state_node( const std::shared_ptr< state_delta >& delta ) noexcept;
  temporary_state_node( const temporary_state_node& ) = delete;
  temporary_state_node( temporary_state_node&& )      = delete;
  ~temporary_state_node() override                    = default;

  temporary_state_node operator=( const temporary_state_node& ) = delete;
  temporary_state_node operator=( temporary_state_node&& )      = delete;

  /**
   * Squash the node in to the parent node. This call invalidates this state node.
   */
  void squash();

private:
  std::shared_ptr< state_delta > mutable_delta() override;
  const std::shared_ptr< state_delta >& delta() const override;

  std::shared_ptr< state_delta > _delta;
};

} // namespace respublica::state_db
