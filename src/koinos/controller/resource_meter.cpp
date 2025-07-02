#include <koinos/controller/resource_meter.hpp>
#include <koinos/controller/session.hpp>

#include <boost/multiprecision/cpp_int.hpp>

namespace koinos::controller {

/*
 * Resource session
 */

resource_session::resource_session( std::uint64_t initial_resources ):
    _initial_resources( initial_resources ),
    _remaining_resources( initial_resources )
{}

std::error_code resource_session::use_resources( std::uint64_t resources )
{
  if( resources > _remaining_resources )
    return reversion_errc::insufficient_resources;

  _remaining_resources -= resources;

  return controller_errc::ok;
}

std::uint64_t resource_session::remaining_resources()
{
  return _remaining_resources;
}

std::uint64_t resource_session::used_resources()
{
  return _initial_resources - _remaining_resources;
}

/*
 * Resource meter
 */

resource_meter::resource_meter()
{
  state::resource_limits initial_limits;

  initial_limits.disk_storage_limit      = std::numeric_limits< std::uint64_t >::max();
  initial_limits.network_bandwidth_limit = std::numeric_limits< std::uint64_t >::max();
  initial_limits.compute_bandwidth_limit = std::numeric_limits< std::uint64_t >::max();

  set_resource_limits( initial_limits );
}

void resource_meter::set_resource_limits( const state::resource_limits& r ) noexcept
{
  _resource_limits             = r;
  _system_use                  = resource_state();
  _remaining.disk_storage      = _resource_limits.disk_storage_limit;
  _remaining.network_bandwidth = _resource_limits.network_bandwidth_limit;
  _remaining.compute_bandwidth = _resource_limits.compute_bandwidth_limit;
}

const state::resource_limits& resource_meter::resource_limits() const noexcept
{
  return _resource_limits;
}

void resource_meter::set_session( const std::shared_ptr< resource_session >& s ) noexcept
{
  _session = s;
}

std::error_code resource_meter::use_disk_storage( std::uint64_t bytes )
{
  if( bytes >= _remaining.disk_storage )
    return controller_errc::disk_storage_limit_exceeded;

  if( auto session = _session.lock() )
  {
    boost::multiprecision::uint128_t resource_cost =
      boost::multiprecision::uint128_t( bytes ) * _resource_limits.disk_storage_cost;

    if( resource_cost > std::numeric_limits< std::uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    if( auto error = session->use_resources( resource_cost.convert_to< std::uint64_t >() ); error )
      return error;
  }
  else
    _system_use.disk_storage += bytes;

  _remaining.disk_storage -= bytes;

  return controller_errc::ok;
}

std::error_code resource_meter::use_network_bandwidth( std::uint64_t bytes )
{
  if( bytes >= _remaining.network_bandwidth )
    return controller_errc::network_bandwidth_limit_exceeded;

  if( auto session = _session.lock() )
  {
    boost::multiprecision::uint128_t resource_cost =
      boost::multiprecision::uint128_t( bytes ) * _resource_limits.network_bandwidth_cost;

    if( resource_cost > std::numeric_limits< std::uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    if( auto error = session->use_resources( resource_cost.convert_to< std::uint64_t >() ); error )
      return error;
  }
  else
    _system_use.network_bandwidth += bytes;

  _remaining.network_bandwidth -= bytes;

  return controller_errc::ok;
}

std::error_code resource_meter::use_compute_bandwidth( std::uint64_t ticks )
{
  if( ticks > _remaining.compute_bandwidth )
    return controller_errc::compute_bandwidth_limit_exceeded;

  if( auto session = _session.lock() )
  {
    boost::multiprecision::uint128_t resource_cost =
      boost::multiprecision::uint128_t( ticks ) * _resource_limits.compute_bandwidth_cost;

    if( resource_cost > std::numeric_limits< std::uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    if( auto error = session->use_resources( resource_cost.convert_to< std::uint64_t >() ); error )
      return error;
  }
  else
    _system_use.compute_bandwidth += ticks;

  _remaining.compute_bandwidth -= ticks;

  return controller_errc::ok;
}

std::uint64_t resource_meter::remaining_disk_storage() const noexcept
{
  if( auto session = _session.lock() )
    return std::min( _remaining.disk_storage, session->remaining_resources() / _resource_limits.disk_storage_cost );

  return _remaining.disk_storage;
}

std::uint64_t resource_meter::remaining_network_bandwidth() const noexcept
{
  if( auto session = _session.lock() )
    return std::min( _remaining.network_bandwidth, session->remaining_resources() / _resource_limits.network_bandwidth_cost );

  return _remaining.network_bandwidth;
}

std::uint64_t resource_meter::remaining_compute_bandwidth() const noexcept
{
  if( auto session = _session.lock() )
    return std::min( _remaining.compute_bandwidth, session->remaining_resources() / _resource_limits.compute_bandwidth_cost );

  return _remaining.compute_bandwidth;
}

const resource_state& resource_meter::remaining_resources() const noexcept
{
  return _remaining;
}

const resource_state& resource_meter::system_resources() const noexcept
{
  return _system_use;
}

} // namespace koinos::controller
