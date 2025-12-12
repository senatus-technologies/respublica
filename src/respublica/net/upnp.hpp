#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <boost/asio.hpp>

#include <respublica/log.hpp>

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
  std::optional< std::string >
  add_port_mapping( std::uint16_t internal_port, std::uint16_t external_port, const std::string& protocol = "TCP" );

  // Remove a port mapping
  void remove_port_mapping( std::uint16_t external_port, const std::string& protocol = "TCP" );

  // Get external IP address
  std::optional< std::string > get_external_ip();

private:
  void discover_gateway();
  std::optional< std::string > parse_location( const std::string& response );
  std::optional< std::string > get_control_url( const std::string& location );
  std::optional< std::string > parse_control_url_from_xml( const std::string& xml );
  std::optional< std::string > get_local_ip();
  bool http_get( const std::string& host, std::uint16_t port, const std::string& path, std::string& response );
  bool send_soap_request( const std::string& control_url,
                          const std::string& action,
                          const std::string& service_type,
                          const std::string& body,
                          std::string& response );

  boost::asio::io_context& _io_context;
  std::optional< std::string > _gateway_url;
  std::optional< std::string > _control_url;
  std::optional< std::string > _service_type;
  std::optional< std::string > _external_ip;
  std::uint16_t _mapped_port{ 0 };
  std::string _protocol;
};

} // namespace respublica::net
