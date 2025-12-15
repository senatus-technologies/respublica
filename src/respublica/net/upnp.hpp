#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <boost/asio.hpp>

#include <respublica/log.hpp>
#include <respublica/net/error.hpp>

namespace respublica::net {

// UPnP manager for automatic port forwarding
class upnp
{
public:
  upnp( boost::asio::io_context& io_context );
  ~upnp();

  upnp( const upnp& )            = delete;
  upnp& operator=( const upnp& ) = delete;
  upnp( upnp&& )                 = delete;
  upnp& operator=( upnp&& )      = delete;

  // Attempt to add a port mapping
  // Returns the external IP if successful
  result< std::string >
  add_port_mapping( std::uint16_t internal_port, std::uint16_t external_port, const std::string& protocol = "TCP" );

  // Remove a port mapping
  std::error_code remove_port_mapping( std::uint16_t external_port, const std::string& protocol = "TCP" );

  // Get external IP address
  result< std::string > get_external_ip();

private:
  void discover_gateway();
  result< std::string > parse_location( const std::string& response );
  result< std::string > get_control_url( const std::string& location );
  result< std::string > parse_control_url_from_xml( const std::string& xml );
  result< std::string > get_local_ip();
  result< std::string > http_get( const std::string& host, std::uint16_t port, const std::string& path );
  result< std::string > send_soap_request( const std::string& control_url,
                                           const std::string& action,
                                           const std::string& service_type,
                                           const std::string& body );

  boost::asio::io_context& _io_context;
  std::optional< std::string > _gateway_url;
  std::optional< std::string > _control_url;
  std::optional< std::string > _service_type;
  std::optional< std::string > _external_ip;
  std::uint16_t _mapped_port{ 0 };
  std::string _protocol;
};

} // namespace respublica::net
