#pragma once

#include <respublica/controller/error.hpp>
#include <respublica/controller/state.hpp>

#include <memory>

namespace respublica::controller {

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

  void set_resource_limits( const state::resource_limits& rld ) noexcept;
  const state::resource_limits& resource_limits() const noexcept;

  void set_session( const std::shared_ptr< resource_session >& s ) noexcept;

  std::error_code use_disk_storage( std::uint64_t bytes );
  std::error_code use_network_bandwidth( std::uint64_t bytes );
  std::error_code use_compute_bandwidth( std::uint64_t ticks );

  std::uint64_t remaining_disk_storage() const noexcept;
  std::uint64_t remaining_network_bandwidth() const noexcept;
  std::uint64_t remaining_compute_bandwidth() const noexcept;

  const resource_state& remaining_resources() const noexcept;
  const resource_state& system_resources() const noexcept;

private:
  resource_state _remaining;
  resource_state _system_use;
  state::resource_limits _resource_limits;
  std::weak_ptr< resource_session > _session;
};

} // namespace respublica::controller
