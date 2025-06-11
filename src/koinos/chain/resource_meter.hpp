#pragma once

#include <koinos/chain/state.hpp>
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
  uint64_t disk_storage      = 0;
  uint64_t network_bandwidth = 0;
  uint64_t compute_bandwidth = 0;
};

class resource_meter final
{
public:
  resource_meter();
  resource_meter( const resource_meter& ) = default;
  resource_meter( resource_meter&& ) = default;
  ~resource_meter() = default;

  resource_meter& operator =( const resource_meter& ) = default;
  resource_meter& operator =( resource_meter&& ) = default;

  void set_resource_limits( const state::resource_limits& rld );
  const state::resource_limits& resource_limits() const;

  void set_session( const std::shared_ptr< rc_session >& s );

  error use_disk_storage( uint64_t bytes );
  error use_network_bandwidth( uint64_t bytes );
  error use_compute_bandwidth( uint64_t ticks );

  const resource_state& remaining_resources() const;
  const resource_state& system_resources() const;

private:
  resource_state _remaining;
  resource_state _system_use;
  state::resource_limits _resource_limits;
  std::weak_ptr< rc_session > _session;
};

} // namespace koinos::chain
