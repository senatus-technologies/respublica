#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_iterator.hpp>

namespace koinos::state_db::backends::map {

class map_backend final: public abstract_backend
{
public:
  map_backend();
  map_backend( uint64_t, state_node_id, protocol::block_header );
  virtual ~map_backend() override;

  // Iterators
  virtual iterator begin() noexcept override;
  virtual iterator end() noexcept override;

  // Modifiers
  virtual void put( std::vector< std::byte >&& key, value_type value ) override;
  virtual void put( std::vector< std::byte >&& key, std::vector< std::byte >&& value ) override;
  virtual std::optional< value_type > get( const std::vector< std::byte >& key ) const override;
  virtual void erase( const std::vector< std::byte >& key ) override;
  virtual void clear() noexcept override;

  virtual uint64_t size() const noexcept override;

  virtual void start_write_batch() override;
  virtual void end_write_batch() override;

  virtual void store_metadata() override;

  virtual std::shared_ptr< abstract_backend > clone() const override;

private:
  std::map< std::vector< std::byte >, std::vector< std::byte > > _map;
  protocol::block_header _header;
};

} // namespace koinos::state_db::backends::map
