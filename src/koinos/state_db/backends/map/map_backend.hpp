#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_iterator.hpp>

namespace koinos::state_db::backends::map {

class map_backend final: public abstract_backend
{
public:
  map_backend();
  map_backend( const state_node_id& id, uint64_t revision );
  virtual ~map_backend() override;

  // Iterators
  virtual iterator begin() noexcept override;
  virtual iterator end() noexcept override;

  // Modifiers
  virtual int64_t put( std::vector< std::byte >&& key, std::span< const std::byte > value ) override;
  virtual int64_t put( std::vector< std::byte >&& key, std::vector< std::byte >&& value ) override;
  virtual std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& key ) const override;
  virtual int64_t remove( const std::vector< std::byte >& key ) override;
  virtual void clear() noexcept override;

  virtual uint64_t size() const noexcept override;

  virtual void start_write_batch() override;
  virtual void end_write_batch() override;

  virtual void store_metadata() override;

  virtual std::shared_ptr< abstract_backend > clone() const override;

private:
  std::map< std::vector< std::byte >, std::vector< std::byte > > _map;
};

} // namespace koinos::state_db::backends::map
