#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_iterator.hpp>

namespace koinos::state_db::backends::map {

class map_backend final: public abstract_backend
{
public:
  map_backend();
  map_backend( const map_backend& )            = default;
  map_backend( map_backend&& )                 = delete;
  map_backend& operator=( const map_backend& ) = default;
  map_backend& operator=( map_backend&& )      = delete;
  map_backend( const state_node_id& id, std::uint64_t revision );
  ~map_backend() final;

  // Iterators
  iterator begin() noexcept final;
  iterator end() noexcept final;

  // Modifiers
  std::int64_t put( std::vector< std::byte >&& key, std::span< const std::byte > value ) final;
  std::int64_t put( std::vector< std::byte >&& key, std::vector< std::byte >&& value ) final;
  std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& key ) const final;
  std::int64_t remove( const std::vector< std::byte >& key ) final;
  void clear() noexcept final;

  std::uint64_t size() const noexcept final;

  void start_write_batch() final;
  void end_write_batch() final;

  void store_metadata() final;

  std::shared_ptr< abstract_backend > clone() const final;

private:
  std::map< std::vector< std::byte >, std::vector< std::byte > > _map;
};

} // namespace koinos::state_db::backends::map
