#pragma once
#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/bytes_less.hpp>
#include <koinos/state_db/types.hpp>

#include <koinos/crypto/crypto.hpp>

#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace koinos::state_db::detail {

class state_delta: public std::enable_shared_from_this< state_delta >
{
private:
  std::shared_ptr< state_delta > _parent;

  std::shared_ptr< backends::abstract_backend > _backend;
  std::set< std::vector< std::byte > > _removed_objects;

  state_node_id _id;
  uint64_t _revision = 0;
  mutable std::optional< digest > _merkle_root;

  bool _finalized = false;

public:
  state_delta() = default;
  state_delta( const std::optional< std::filesystem::path >& p );
  ~state_delta() = default;

  void put( std::vector< std::byte >&& key, value_type value );
  void erase( std::vector< std::byte >&& key );
  std::optional< value_type > find( const std::vector< std::byte >& key ) const;

  void squash();
  void commit();

  void clear();

  bool is_modified( const std::vector< std::byte >& key ) const;
  bool is_removed( const std::vector< std::byte >& key ) const;
  bool is_root() const;
  bool is_empty() const;

  uint64_t revision() const;
  void set_revision( uint64_t revision );

  bool is_finalized() const;
  void finalize();

  const digest& merkle_root() const;

  const state_node_id& id() const;
  const state_node_id& parent_id() const;
  std::shared_ptr< state_delta > parent() const;
  const protocol::block_header& block_header() const;

  std::shared_ptr< state_delta > make_child( const state_node_id& id              = state_node_id(),
                                             const protocol::block_header& header = protocol::block_header() );
  std::shared_ptr< state_delta > clone( const state_node_id& id, const protocol::block_header& header );

  const std::shared_ptr< backends::abstract_backend > backend() const;

private:
  void commit_helper();

  std::shared_ptr< state_delta > get_root();
};

} // namespace koinos::state_db::detail
