#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using int128_t = boost::multiprecision::int128_t;

namespace koinos::chain {

/*
 * Resource session
 */

rc_session::rc_session( uint64_t begin_rc ):
  _begin_rc( begin_rc ),
  _end_rc( begin_rc )
{}

error_code rc_session::use_rc( uint64_t rc )
{
  KOINOS_CHECK_ERROR( rc <= _end_rc, error_code::insufficient_rc, "insufficient rc" );
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
  resource_limit_data initial_rld;

  initial_rld.set_disk_storage_limit( std::numeric_limits< uint64_t >::max() );
  initial_rld.set_network_bandwidth_limit( std::numeric_limits< uint64_t >::max() );
  initial_rld.set_compute_bandwidth_limit( std::numeric_limits< uint64_t >::max() );

  set_resource_limit_data( initial_rld );
}

resource_meter::~resource_meter() = default;

void resource_meter::set_resource_limit_data( const resource_limit_data& rld )
{
  _resource_limit_data         = rld;
  _system_use                  = resource_state();
  _remaining.disk_storage      = _resource_limit_data.disk_storage_limit();
  _remaining.network_bandwidth = _resource_limit_data.network_bandwidth_limit();
  _remaining.compute_bandwidth = _resource_limit_data.compute_bandwidth_limit();
}

const resource_limit_data& resource_meter::get_resource_limit_data() const
{
  return _resource_limit_data;
}

void resource_meter::set_session( std::shared_ptr< rc_session > s )
{
  _session = s;
}

error_code resource_meter::use_disk_storage( uint64_t bytes )
{
  KOINOS_CHECK_ERROR(
    bytes <= _remaining.disk_storage,
    error_code::disk_storage_limit_exceeded,
    "disk storage limit exceeded" );

  if( auto session = _session.lock() )
  {
    uint128_t rc_cost = uint128_t( bytes ) * _resource_limit_data.disk_storage_cost();
    if( rc_cost <= std::numeric_limits< uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    session->use_rc( rc_cost.convert_to< uint64_t >() );
  }
  else
  {
    _system_use.disk_storage += bytes;
  }

  _remaining.disk_storage -= uint64_t( bytes );
}

error_code resource_meter::use_network_bandwidth( int64_t bytes )
{
  KOINOS_CHECK_ERROR(
    bytes <= _remaining.disk_storage,
    error_code::network_bandwidth_limit_exceeded,
    "network bandwidth limit exceeded" );

  if( auto session = _session.lock() )
  {
    uint128_t rc_cost = uint128_t( bytes ) * _resource_limit_data.network_bandwidth_cost();
    if( rc_cost <= std::numeric_limits< uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    session->use_rc( rc_cost.convert_to< uint64_t >() );
  }
  else
  {
    _system_use.network_bandwidth += bytes;
  }

  _remaining.network_bandwidth -= uint64_t( bytes );
}

error_code resource_meter::use_compute_bandwidth( int64_t ticks )
{
  KOINOS_CHECK_ERROR(
    bytes <= _remaining.compute_bandwidth,
    error_code::compute_bandwidth_limit_exceeded,
    "compute_bandwidth limit exceeded" );

  if( auto session = _session.lock() )
  {
    uint128_t rc_cost = uint128_t( bytes ) * _resource_limit_data.compute_bandwidth_cost();
    if( rc_cost <= std::numeric_limits< uint64_t >::max() )
      throw std::runtime_error( "rc overflow" );

    session->use_rc( rc_cost.convert_to< uint64_t >() );
  }
  else
  {
    _system_use.compute_bandwidth += bytes;
  }

  _remaining.compute_bandwidth -= uint64_t( bytes );
}

resource_state resource_meter::remaining_resources() const
{
  return _remaining;
}

resource_state resource_meter::system_resources() const
{
  return _system_use;
}

} // namespace koinos::chain
