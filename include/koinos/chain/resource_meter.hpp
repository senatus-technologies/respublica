#pragma once

#include <koinos/chain/chain.pb.h>
#include <koinos/chain/types.hpp>

#include <memory>

namespace koinos::chain {

namespace compute_load {
constexpr uint64_t light  = 100;
constexpr uint64_t medium = 1'000;
constexpr uint64_t heavy  = 10'000;
} // namespace compute_load

struct rc_session
{
  rc_session( uint64_t begin_rc );

  error use_rc( uint64_t rc );
  uint64_t remaining_rc();
  uint64_t used_rc();

private:
  uint64_t _begin_rc;
  uint64_t _end_rc;
};

struct resource_state
{
  uint64_t disk_storage = 0;
  uint64_t network_bandwidth = 0;
  uint64_t compute_bandwidth = 0;
};

class resource_meter final
{
public:
  resource_meter();
  ~resource_meter();

  void set_resource_limit_data( const resource_limit_data& rld );
  const resource_limit_data& get_resource_limit_data() const;

  void set_session( std::shared_ptr< rc_session > s );

  error use_disk_storage( uint64_t bytes );
  error use_network_bandwidth( uint64_t bytes );
  error use_compute_bandwidth( uint64_t ticks );

  resource_state remaining_resources() const;
  resource_state system_resources() const;

private:
  resource_state _remaining;
  resource_state _system_use;
  resource_limit_data _resource_limit_data;
  std::weak_ptr< rc_session > _session;
};

} // namespace koinos::chain
