#pragma once
#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/types.hpp>

#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace koinos::state_db {

class state_delta: public std::enable_shared_from_this< state_delta >
{
private:
  std::shared_ptr< state_delta > _parent;

  std::shared_ptr< backends::abstract_backend > _backend;
  std::set< std::vector< std::byte > > _removed_objects;

  mutable std::optional< digest > _merkle_root;

  bool _final = false;

public:
  state_delta() = default;
  state_delta( const std::optional< std::filesystem::path >& p );
  ~state_delta() = default;

  int64_t put( std::vector< std::byte >&& key, std::span< const std::byte > value );
  int64_t remove( std::vector< std::byte >&& key );
  std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& key ) const;

  void squash();
  void commit();
  void clear();

  bool key_removed( const std::vector< std::byte >& key ) const;
  bool root() const;

  uint64_t revision() const;
  void set_revision( uint64_t revision );

  bool final() const;
  void finalize();

  const digest& merkle_root() const;

  const state_node_id& id() const;
  const state_node_id& parent_id() const;
  std::shared_ptr< state_delta > parent() const;

  std::shared_ptr< state_delta > make_child( const state_node_id& id = null_id );
  std::shared_ptr< state_delta > clone( const state_node_id& id = null_id );

  const std::shared_ptr< backends::abstract_backend > backend() const;

private:
  void commit_helper();
};

} // namespace koinos::state_db
