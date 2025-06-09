#pragma once

#include <koinos/state_db/backends/iterator.hpp>
#include <koinos/state_db/types.hpp>

#include <koinos/crypto/crypto.hpp>

namespace koinos::state_db::backends {

class abstract_backend
{
public:
  abstract_backend();
  abstract_backend( const state_node_id& id, uint64_t revision );
  virtual ~abstract_backend() {};

  virtual iterator begin() = 0;
  virtual iterator end()   = 0;

  virtual int64_t put( std::vector< std::byte >&& key, std::span< const std::byte > value )              = 0;
  virtual int64_t put( std::vector< std::byte >&& key, std::vector< std::byte >&& value )                = 0;
  virtual std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& key ) const = 0;
  virtual int64_t remove( const std::vector< std::byte >& key )                                          = 0;
  virtual void clear()                                                                                   = 0;

  virtual uint64_t size() const = 0;
  bool empty() const;

  uint64_t revision() const;
  void set_revision( uint64_t );

  const state_node_id& id() const;
  void set_id( const state_node_id& );

  const digest& merkle_root() const;
  void set_merkle_root( const digest& );

  virtual void start_write_batch() = 0;
  virtual void end_write_batch()   = 0;

  virtual void store_metadata() = 0;

  virtual std::shared_ptr< abstract_backend > clone() const = 0;

private:
  state_node_id _id{};
  uint64_t _revision = 0;
  crypto::digest _merkle_root{};
};

} // namespace koinos::state_db::backends
