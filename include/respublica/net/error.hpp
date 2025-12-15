#pragma once

#include <expected>
#include <system_error>

namespace respublica::net {

enum class net_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  upnp_gateway_not_found,
  upnp_invalid_url,
  upnp_http_request_failed,
  upnp_xml_parse_error,
  upnp_service_not_found,
  upnp_soap_request_failed,
  upnp_port_mapping_failed,
  connection_failed,
  ssl_handshake_failed,
  certificate_verification_failed,
  invalid_parameters,
  network_error
};

const std::error_category& net_category() noexcept;

std::error_code make_error_code( net_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace respublica::net

template<>
struct std::is_error_code_enum< respublica::net::net_errc >: public std::true_type
{};
