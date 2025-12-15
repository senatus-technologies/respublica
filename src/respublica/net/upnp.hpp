#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/url.hpp>

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
  std::error_code add_port_mapping( std::uint16_t internal_port, std::uint16_t external_port, std::string_view protocol = "TCP" );

  // Remove a port mapping
  std::error_code remove_port_mapping( std::uint16_t external_port, std::string_view protocol = "TCP" );

  // Get external IP address
  result< boost::asio::ip::address > get_external_ip();

private:
  void discover_gateway();
  result< std::string > parse_location( std::string_view response );
  result< boost::urls::url > get_control_url( std::string_view location );
  result< std::string > parse_control_url_from_xml( std::string_view xml );
  result< boost::asio::ip::address > get_local_ip();
  result< std::string > http_get( std::string_view host, std::uint16_t port, std::string_view path );
  result< std::string > send_soap_request( std::string_view control_url,
                                           std::string_view action,
                                           std::string_view service_type,
                                           std::string_view body );

  boost::asio::io_context& _io_context;
  std::optional< boost::urls::url > _gateway_url;
  std::optional< boost::urls::url > _control_url;
  std::optional< std::string > _service_type;
  std::optional< boost::asio::ip::address > _external_ip;
  std::uint16_t _mapped_port{ 0 };
  std::string _protocol;
};

} // namespace respublica::net