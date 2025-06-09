#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using uint128_t = boost::multiprecision::uint128_t;

namespace koinos::chain {

using koinos::error::error_code;

/*
 * Resource session
 */

rc_session::rc_session( uint64_t begin_rc ):
    _begin_rc( begin_rc ),
    _end_rc( begin_rc )
{}

error rc_session::use_rc( uint64_t rc )
{
  if( rc <= _end_rc )
    return error( error_code::insufficient_rc );

  _end_rc -= rc;

  return {};
}

uint64_t rc_session::remaining_rc()
{
  return _end_rc;
}

uint64_t rc_session::used_rc()
{
  return _begin_rc - _end_rc;
}

/*
 * Resource meter
 */

resource_meter::resource_meter()
{
  state::resource_limits initial_limits;

  initial_limits.disk_storage_limit      = std::numeric_limits< uint64_t >::max();
  initial_limits.network_bandwidth_limit = std::numeric_limits< uint64_t >::max();
  initial_limits.compute_bandwidth_limit = std::numeric_limits< uint64_t >::max();

  set_resource_limits( initial_limits );
}

resource_meter::~resource_meter() = default;

void resource_meter::set_resource_limits( const state::resource_limits& r )
{
  _resource_limits             = r;
  _system_use                  = resource_state();
  _remaining.disk_storage      = _resource_limits.disk_storage_limit;
  _remaining.network_bandwidth = _resource_limits.network_bandwidth_limit;
  _remaining.compute_bandwidth = _resource_limits.compute_bandwidth_limit;
}

const state::resource_limits& resource_meter::resource_limits() const
{
  return _resource_limits;
}

void resource_meter::set_session( std::shared_ptr< rc_session > s )
{
  _session = s;
}

error resource_meter::use_disk_storage( uint64_t bytes )
{
  if( bytes >= _remaining.disk_storage )
    return error( error_code::disk_storage_limit_exceeded );

  if( auto session = _session.lock() )
  {
    uint128_t rc_cost = uint128_t( bytes ) * _resource_limits.disk_storage_cost;
    if( rc_cost > std::numeric_limits< uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    session->use_rc( rc_cost.convert_to< uint64_t >() );
  }
  else
    _system_use.disk_storage += bytes;

  _remaining.disk_storage -= uint64_t( bytes );

  return {};
}

error resource_meter::use_network_bandwidth( uint64_t bytes )
{
  if( bytes >= _remaining.network_bandwidth )
    return error( error_code::network_bandwidth_limit_exceeded );

  if( auto session = _session.lock() )
  {
    uint128_t rc_cost = uint128_t( bytes ) * _resource_limits.network_bandwidth_cost;
    if( rc_cost > std::numeric_limits< uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    session->use_rc( rc_cost.convert_to< uint64_t >() );
  }
  else
    _system_use.network_bandwidth += bytes;

  _remaining.network_bandwidth -= uint64_t( bytes );

  return {};
}

error resource_meter::use_compute_bandwidth( uint64_t ticks )
{
  if( ticks > _remaining.compute_bandwidth )
    return error( error_code::compute_bandwidth_limit_exceeded );

  if( auto session = _session.lock() )
  {
    uint128_t rc_cost = uint128_t( ticks ) * _resource_limits.compute_bandwidth_cost;
    if( rc_cost > std::numeric_limits< uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    session->use_rc( rc_cost.convert_to< uint64_t >() );
  }
  else
    _system_use.compute_bandwidth += ticks;

  _remaining.compute_bandwidth -= uint64_t( ticks );

  return {};
}

const resource_state& resource_meter::remaining_resources() const
{
  return _remaining;
}

const resource_state& resource_meter::system_resources() const
{
  return _system_use;
}

} // namespace koinos::chain
