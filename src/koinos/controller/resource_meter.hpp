#pragma once

#include <koinos/controller/error.hpp>
#include <koinos/controller/state.hpp>

#include <memory>

namespace koinos::controller {

struct resource_session
{
  resource_session( std::uint64_t initial_resources );

  std::error_code use_resources( std::uint64_t resources );
  std::uint64_t remaining_resources();
  std::uint64_t used_resources();

private:
  std::uint64_t _initial_resources;
  std::uint64_t _remaining_resources;
};

struct resource_state
{
  std::uint64_t disk_storage      = 0;
  std::uint64_t network_bandwidth = 0;
  std::uint64_t compute_bandwidth = 0;
};

class resource_meter final
{
public:
  resource_meter();
  resource_meter( const resource_meter& ) = default;
  resource_meter( resource_meter&& )      = default;
  ~resource_meter()                       = default;

  resource_meter& operator=( const resource_meter& ) = default;
  resource_meter& operator=( resource_meter&& )      = default;

  void set_resource_limits( const state::resource_limits& rld );
  const state::resource_limits& resource_limits() const;

  void set_session( const std::shared_ptr< resource_session >& s );

  std::error_code use_disk_storage( std::uint64_t bytes );
  std::error_code use_network_bandwidth( std::uint64_t bytes );
  std::error_code use_compute_bandwidth( std::uint64_t ticks );

  const resource_state& remaining_resources() const;
  const resource_state& system_resources() const;

private:
  resource_state _remaining;
  resource_state _system_use;
  state::resource_limits _resource_limits;
  std::weak_ptr< resource_session > _session;
};

} // namespace koinos::controller
