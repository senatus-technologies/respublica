#include <respublica/net/error.hpp>
#include <string>
#include <utility>

namespace respublica::net {

struct _net_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "net";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< net_errc >( condition ) )
    {
      case net_errc::ok:
        return "ok"s;
      case net_errc::upnp_gateway_not_found:
        return "UPnP gateway not found"s;
      case net_errc::upnp_invalid_url:
        return "invalid UPnP URL"s;
      case net_errc::upnp_http_request_failed:
        return "UPnP HTTP request failed"s;
      case net_errc::upnp_xml_parse_error:
        return "UPnP XML parse error"s;
      case net_errc::upnp_service_not_found:
        return "UPnP service not found"s;
      case net_errc::upnp_soap_request_failed:
        return "UPnP SOAP request failed"s;
      case net_errc::upnp_port_mapping_failed:
        return "UPnP port mapping failed"s;
      case net_errc::connection_failed:
        return "connection failed"s;
      case net_errc::ssl_handshake_failed:
        return "SSL handshake failed"s;
      case net_errc::certificate_verification_failed:
        return "certificate verification failed"s;
      case net_errc::invalid_parameters:
        return "invalid parameters"s;
      case net_errc::network_error:
        return "network error"s;
    }
    std::unreachable();
  }
};

const std::error_category& net_category() noexcept
{
  static _net_category category;
  return category;
}

std::error_code make_error_code( net_errc e )
{
  return std::error_code( static_cast< int >( e ), net_category() );
}

} // namespace respublica::net
