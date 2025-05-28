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
  virtual void put( key_type k, value_type v ) override;
  virtual std::optional< value_type > get( key_type ) const override;
  virtual void erase( key_type k ) override;
  virtual void clear() noexcept override;

  virtual uint64_t size() const noexcept override;

  virtual void start_write_batch() override;
  virtual void end_write_batch() override;

  virtual void store_metadata() override;

  virtual std::shared_ptr< abstract_backend > clone() const override;

private:
  map_type _map;
  span_type _span_map;
  protocol::block_header _header;
};

} // namespace koinos::state_db::backends::map
