#pragma once

#include <koinos/state_db/types.hpp>
#include <koinos/state_db/backends/iterator.hpp>

#include <koinos/crypto/crypto.hpp>
#include <koinos/protocol/protocol.hpp>

namespace koinos::state_db::backends {

class abstract_backend
{
public:
  abstract_backend();
  abstract_backend( uint64_t, state_node_id, protocol::block_header );
  virtual ~abstract_backend() {};

  virtual iterator begin() = 0;
  virtual iterator end()   = 0;

  virtual void put( std::vector< std::byte >&& key, std::span< const std::byte > value )                 = 0;
  virtual void put( std::vector< std::byte >&& key, std::vector< std::byte >&& value ) = 0;
  virtual std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& key ) const = 0;
  virtual void erase( const std::vector< std::byte >& key )                            = 0;
  virtual void clear()                                                                 = 0;

  virtual uint64_t size() const = 0;
  bool empty() const;

  uint64_t revision() const;
  void set_revision( uint64_t );

  const state_node_id& id() const;
  void set_id( const state_node_id& );

  const crypto::digest& merkle_root() const;
  void set_merkle_root( const crypto::digest& );

  const protocol::block_header& block_header() const;
  void set_block_header( const protocol::block_header& );

  virtual void start_write_batch() = 0;
  virtual void end_write_batch()   = 0;

  virtual void store_metadata() = 0;

  virtual std::shared_ptr< abstract_backend > clone() const = 0;

private:
  uint64_t _revision = 0;
  state_node_id _id;
  crypto::digest _merkle_root;
  protocol::block_header _header;
};

} // namespace koinos::state_db::backends
