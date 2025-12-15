#include <respublica/log.hpp>
#include <respublica/memory.hpp>
#include <respublica/net/upnp.hpp>

#include <chrono>
#include <format>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/url.hpp>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

namespace respublica::net {

constexpr const char* upnp_multicast_addr   = "239.255.255.250";
constexpr std::uint16_t upnp_multicast_port = 1'900;
constexpr auto upnp_discovery_timeout       = std::chrono::seconds( 3 );
constexpr std::size_t ssdp_recv_buffer_size = 2'048;
constexpr auto ssdp_poll_interval           = std::chrono::milliseconds( 100 );
constexpr std::uint16_t http_default_port   = 80;

// SSDP discovery message
constexpr const char* ssdp_discover_msg = "M-SEARCH * HTTP/1.1\r\n"
                                          "HOST: 239.255.255.250:1900\r\n"
                                          "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
                                          "MAN: \"ssdp:discover\"\r\n"
                                          "MX: 3\r\n"
                                          "\r\n";

upnp::upnp( boost::asio::io_context& io_context ):
    _io_context( io_context )
{
  LOG_INFO( respublica::log::instance(), "Initializing UPnP manager" );

  // Initialize libxml2
  xmlInitParser();
  LIBXML_TEST_VERSION

  discover_gateway();
}

upnp::~upnp()
{
  if( _mapped_port > 0 )
  {
    LOG_INFO( respublica::log::instance(), "Cleaning up UPnP port mapping for port {}", _mapped_port );
    remove_port_mapping( _mapped_port, _protocol );
  }

  // Cleanup libxml2
  xmlCleanupParser();
}

void upnp::discover_gateway()
{
  try
  {
    LOG_INFO( respublica::log::instance(), "Discovering UPnP gateway via SSDP" );

    boost::asio::ip::udp::socket socket( _io_context );
    socket.open( boost::asio::ip::udp::v4() );
    socket.set_option( boost::asio::socket_base::broadcast( true ) );

    boost::asio::ip::udp::endpoint multicast_endpoint( boost::asio::ip::make_address( upnp_multicast_addr ),
                                                       upnp_multicast_port );

    socket.send_to( boost::asio::buffer( ssdp_discover_msg, std::strlen( ssdp_discover_msg ) ), multicast_endpoint );

    socket.non_blocking( true );

    std::array< char, ssdp_recv_buffer_size > recv_buffer{};
    boost::asio::ip::udp::endpoint sender_endpoint;

    auto start_time = std::chrono::steady_clock::now();
    bool found      = false;

    while( !found && ( std::chrono::steady_clock::now() - start_time ) < upnp_discovery_timeout )
    {
      boost::system::error_code ec;
      std::size_t len = socket.receive_from( boost::asio::buffer( recv_buffer ), sender_endpoint, 0, ec );

      if( !ec && len > 0 )
      {
        std::string response( recv_buffer.data(), len );
        auto location = parse_location( response );

        if( location )
        {
          LOG_INFO( respublica::log::instance(), "Found UPnP gateway at: {}", *location );
          _gateway_url     = *location;
          auto control_url = get_control_url( *location );
          if( control_url )
          {
            _control_url = *control_url;
          }
          found = true;
        }
      }
      else if( ec == boost::asio::error::would_block )
      {
        std::this_thread::sleep_for( ssdp_poll_interval );
      }
      else if( ec )
      {
        LOG_ERROR( respublica::log::instance(), "UPnP discovery receive error: {}", ec.message() );
        break;
      }
    }

    if( !found )
    {
      LOG_WARNING( respublica::log::instance(), "No UPnP gateway found" );
    }
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "UPnP discovery failed: {}", e.what() );
  }
}

result< std::string > upnp::parse_location( const std::string& response )
{
  std::string_view response_view( response );
  std::size_t pos = 0;

  while( pos < response_view.size() )
  {
    auto line_end = response_view.find( '\n', pos );
    if( line_end == std::string_view::npos )
    {
      line_end = response_view.size();
    }

    std::string_view line = response_view.substr( pos, line_end - pos );

    // Remove trailing \r if present
    if( !line.empty() && line.back() == '\r' )
    {
      line = line.substr( 0, line.size() - 1 );
    }

    if( line.starts_with( "LOCATION:" ) || line.starts_with( "Location:" ) )
    {
      auto colon_pos = line.find( ':' );
      if( colon_pos != std::string_view::npos )
      {
        std::string_view location = line.substr( colon_pos + 1 );

        // Trim whitespace
        auto start = location.find_first_not_of( " \t\r\n" );
        if( start != std::string_view::npos )
        {
          location = location.substr( start );
          auto end = location.find_last_not_of( " \t\r\n" );
          if( end != std::string_view::npos )
          {
            location = location.substr( 0, end + 1 );
          }
        }

        return std::string( location );
      }
    }

    pos = line_end + 1;
  }

  return std::unexpected( net_errc::upnp_gateway_not_found );
}

result< std::string > upnp::get_control_url( const std::string& location )
{
  try
  {
    // Parse URL to extract host and path
    auto url_result = boost::urls::parse_uri( location );
    if( !url_result )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid gateway URL format: {}", location );
      return std::unexpected( net_errc::upnp_invalid_url );
    }

    const boost::urls::url_view& url = *url_result;
    std::string scheme               = url.scheme();
    std::string host                 = url.host();
    std::string path                 = url.path().empty() ? "/" : std::string( url.path() );

    std::uint16_t port = url.has_port() ? url.port_number() : http_default_port;

    // Fetch device description XML
    auto xml_response = http_get( host, port, path );
    if( !xml_response )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to fetch device description XML" );
      return std::unexpected( xml_response.error() );
    }

    // Extract XML body from HTTP response
    constexpr std::size_t HTTP_HEADER_SEPARATOR_LENGTH = 4;
    auto body_start                                    = xml_response->find( "\r\n\r\n" );
    if( body_start != std::string::npos )
    {
      *xml_response = xml_response->substr( body_start + HTTP_HEADER_SEPARATOR_LENGTH );
    }

    // Parse control URL from XML
    auto control_url = parse_control_url_from_xml( *xml_response );
    if( !control_url )
    {
      LOG_WARNING( respublica::log::instance(), "Could not parse control URL from device description" );
      return std::unexpected( control_url.error() );
    }

    // Make control URL absolute if it's relative
    if( !control_url->empty() && control_url->front() == '/' )
    {
      return std::format( "{}://{}:{}{}", scheme, host, port, *control_url );
    }

    return control_url;
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "Error getting control URL: {}", e.what() );
    return std::unexpected( net_errc::upnp_http_request_failed );
  }
}

result< std::string > upnp::parse_control_url_from_xml( const std::string& xml )
{
  if( xml.empty() )
  {
    LOG_ERROR( respublica::log::instance(), "Empty XML device description" );
    return std::unexpected( net_errc::upnp_xml_parse_error );
  }

  xmlDocPtr doc = xmlReadMemory( xml.c_str(),
                                 static_cast< int >( xml.length() ),
                                 nullptr,
                                 nullptr,
                                 XML_PARSE_NOWARNING | XML_PARSE_NOERROR | XML_PARSE_RECOVER );
  if( !doc )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to parse XML device description" );
    return std::unexpected( net_errc::upnp_xml_parse_error );
  }

  xmlXPathContextPtr xpath_ctx = xmlXPathNewContext( doc );
  if( !xpath_ctx )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to create XPath context" );
    xmlFreeDoc( doc );
    return std::unexpected( net_errc::upnp_xml_parse_error );
  }

  std::optional< std::string > control_url;
  std::vector< std::string > service_types = { "WANIPConnection", "WANPPPConnection" };

  for( const auto& service_type: service_types )
  {
    // XPath to find service with matching type (ignoring namespaces)
    std::string xpath =
      "//*[local-name()='serviceType' and starts-with(text(), 'urn:schemas-upnp-org:service:" + service_type + ":')]";
    xmlXPathObjectPtr xpath_obj =
      xmlXPathEvalExpression( memory::pointer_cast< const xmlChar* >( xpath.c_str() ), xpath_ctx );

    if( !xpath_obj )
    {
      LOG_WARNING( respublica::log::instance(), "XPath evaluation failed for service type: {}", service_type );
      continue;
    }

    if( xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0 )
    {
      xmlNodePtr service_type_node = *xpath_obj->nodesetval->nodeTab;
      xmlChar* service_type_text   = xmlNodeGetContent( service_type_node );

      if( service_type_text )
      {
        _service_type = std::string( memory::pointer_cast< const char* >( service_type_text ) );
        xmlFree( service_type_text );

        LOG_INFO( respublica::log::instance(), "Found service type: {}", *_service_type );

        // Get the parent <service> node
        xmlNodePtr service_node = service_type_node->parent;
        if( service_node )
        {
          // Find controlURL within this service
          for( xmlNodePtr child = service_node->children; child; child = child->next )
          {
            if( child->type == XML_ELEMENT_NODE
                && xmlStrcmp( child->name, memory::pointer_cast< const xmlChar* >( "controlURL" ) ) == 0 )
            {
              xmlChar* url_text = xmlNodeGetContent( child );
              if( url_text )
              {
                std::string url_str( memory::pointer_cast< const char* >( url_text ) );
                xmlFree( url_text );

                // Validate URL is not empty
                if( !url_str.empty() )
                {
                  control_url = url_str;
                  LOG_INFO( respublica::log::instance(), "Found control URL: {}", *control_url );
                  break;
                }
              }
            }
          }
        }

        xmlXPathFreeObject( xpath_obj );
        break;
      }
      else
      {
        LOG_WARNING( respublica::log::instance(), "Failed to get service type content" );
      }
    }

    xmlXPathFreeObject( xpath_obj );
  }

  xmlXPathFreeContext( xpath_ctx );
  xmlFreeDoc( doc );

  if( !control_url )
  {
    LOG_WARNING( respublica::log::instance(),
                 "No WANIPConnection or WANPPPConnection service found in device description" );
    return std::unexpected( net_errc::upnp_service_not_found );
  }

  return *control_url;
}

result< std::string > upnp::http_get( const std::string& host, std::uint16_t port, const std::string& path )
{
  try
  {
    if( host.empty() || path.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid HTTP GET parameters: host or path is empty" );
      return std::unexpected( net_errc::invalid_parameters );
    }

    boost::asio::ip::tcp::resolver resolver( _io_context );
    boost::asio::ip::tcp::socket socket( _io_context );

    boost::system::error_code ec;
    auto endpoints = resolver.resolve( host, std::to_string( port ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to resolve host {}: {}", host, ec.message() );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    boost::asio::connect( socket, endpoints, ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to connect to {}:{}: {}", host, port, ec.message() );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    // Send HTTP GET request
    std::string request = std::format( "GET {} HTTP/1.1\r\n"
                                       "Host: {}\r\n"
                                       "Connection: close\r\n"
                                       "User-Agent: Respublica/1.0\r\n"
                                       "\r\n",
                                       path,
                                       host );
    boost::asio::write( socket, boost::asio::buffer( request ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to send HTTP request: {}", ec.message() );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    // Read response with timeout handling
    boost::asio::streambuf response_buf;
    boost::asio::read_until( socket, response_buf, "\r\n\r\n", ec );
    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to read HTTP headers: {}", ec.message() );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    // Read remaining data
    while( boost::asio::read( socket, response_buf, boost::asio::transfer_at_least( 1 ), ec ) )
      ;

    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "HTTP GET error: {}", ec.message() );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    std::string response( boost::asio::buffers_begin( response_buf.data() ),
                          boost::asio::buffers_end( response_buf.data() ) );

    // Validate we got some response
    if( response.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Empty HTTP response received" );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    // Check for HTTP success
    if( response.find( "HTTP/1." ) != 0 )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid HTTP response format" );
      return std::unexpected( net_errc::upnp_http_request_failed );
    }

    return response;
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "HTTP GET exception: {}", e.what() );
    return std::unexpected( net_errc::upnp_http_request_failed );
  }
}

result< std::string >
upnp::add_port_mapping( std::uint16_t internal_port, std::uint16_t external_port, const std::string& protocol )
{
  if( !_control_url )
  {
    LOG_WARNING( respublica::log::instance(), "Cannot add port mapping: No UPnP gateway found" );
    return std::unexpected( net_errc::upnp_gateway_not_found );
  }

  LOG_INFO( respublica::log::instance(),
            "Adding UPnP port mapping: external {} -> internal {} ({})",
            external_port,
            internal_port,
            protocol );

  // Get local IP address
  auto local_ip = get_local_ip();
  if( !local_ip )
  {
    LOG_ERROR( respublica::log::instance(), "Could not determine local IP address" );
    return std::unexpected( local_ip.error() );
  }

  // Build SOAP request
  std::string service_type_urn = _service_type ? *_service_type : "urn:schemas-upnp-org:service:WANIPConnection:1";

  std::string body = std::format( "<?xml version=\"1.0\"?>\r\n"
                                  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                  "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                                  "<s:Body>\r\n"
                                  "<u:AddPortMapping xmlns:u=\"{}\">\r\n"
                                  "<NewRemoteHost></NewRemoteHost>\r\n"
                                  "<NewExternalPort>{}</NewExternalPort>\r\n"
                                  "<NewProtocol>{}</NewProtocol>\r\n"
                                  "<NewInternalPort>{}</NewInternalPort>\r\n"
                                  "<NewInternalClient>{}</NewInternalClient>\r\n"
                                  "<NewEnabled>1</NewEnabled>\r\n"
                                  "<NewPortMappingDescription>Respublica P2P</NewPortMappingDescription>\r\n"
                                  "<NewLeaseDuration>0</NewLeaseDuration>\r\n"
                                  "</u:AddPortMapping>\r\n"
                                  "</s:Body>\r\n"
                                  "</s:Envelope>\r\n",
                                  service_type_urn,
                                  external_port,
                                  protocol,
                                  internal_port,
                                  *local_ip );

  auto response = send_soap_request( *_control_url, "AddPortMapping", service_type_urn, body );
  if( !response )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to add port mapping via SOAP" );
    return std::unexpected( net_errc::upnp_port_mapping_failed );
  }

  // Store for cleanup
  _mapped_port = external_port;
  _protocol    = protocol;

  LOG_INFO( respublica::log::instance(), "UPnP port mapping added successfully" );
  return get_external_ip();
}

std::error_code upnp::remove_port_mapping( std::uint16_t external_port, const std::string& protocol )
{
  if( !_control_url )
  {
    return net_errc::upnp_gateway_not_found;
  }

  LOG_INFO( respublica::log::instance(), "Removing UPnP port mapping for port {} ({})", external_port, protocol );

  std::string service_type_urn = _service_type ? *_service_type : "urn:schemas-upnp-org:service:WANIPConnection:1";

  std::string body = std::format( "<?xml version=\"1.0\"?>\r\n"
                                  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                  "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                                  "<s:Body>\r\n"
                                  "<u:DeletePortMapping xmlns:u=\"{}\">\r\n"
                                  "<NewRemoteHost></NewRemoteHost>\r\n"
                                  "<NewExternalPort>{}</NewExternalPort>\r\n"
                                  "<NewProtocol>{}</NewProtocol>\r\n"
                                  "</u:DeletePortMapping>\r\n"
                                  "</s:Body>\r\n"
                                  "</s:Envelope>\r\n",
                                  service_type_urn,
                                  external_port,
                                  protocol );

  auto response = send_soap_request( *_control_url, "DeletePortMapping", service_type_urn, body );
  if( !response )
  {
    return response.error();
  }

  _mapped_port = 0;
  return {};
}

result< std::string > upnp::get_external_ip()
{
  if( _external_ip )
  {
    return *_external_ip;
  }

  if( !_control_url )
  {
    return std::unexpected( net_errc::upnp_gateway_not_found );
  }

  LOG_INFO( respublica::log::instance(), "Querying external IP via UPnP" );

  std::string service_type_urn = _service_type ? *_service_type : "urn:schemas-upnp-org:service:WANIPConnection:1";

  std::string body = std::format( "<?xml version=\"1.0\"?>\r\n"
                                  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                  "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                                  "<s:Body>\r\n"
                                  "<u:GetExternalIPAddress xmlns:u=\"{}\">\r\n"
                                  "</u:GetExternalIPAddress>\r\n"
                                  "</s:Body>\r\n"
                                  "</s:Envelope>\r\n",
                                  service_type_urn );

  auto response = send_soap_request( *_control_url, "GetExternalIPAddress", service_type_urn, body );
  if( !response )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to get external IP via SOAP" );
    return std::unexpected( response.error() );
  }

  // Parse IP from response
  constexpr std::size_t new_external_ip_tag_length = 22;
  auto ip_start                                    = response->find( "<NewExternalIPAddress>" );
  if( ip_start != std::string::npos )
  {
    ip_start    += new_external_ip_tag_length;
    auto ip_end  = response->find( "</NewExternalIPAddress>", ip_start );

    if( ip_end != std::string::npos )
    {
      _external_ip = response->substr( ip_start, ip_end - ip_start );
      LOG_INFO( respublica::log::instance(), "External IP: {}", *_external_ip );
      return *_external_ip;
    }
  }

  return std::unexpected( net_errc::upnp_soap_request_failed );
}

result< std::string > upnp::get_local_ip()
{
  try
  {
    // Create a UDP socket and connect to a public address
    // This doesn't actually send data, but allows us to query the local endpoint
    constexpr const char* public_dns_server = "8.8.8.8";
    boost::asio::ip::udp::socket socket( _io_context );
    socket.open( boost::asio::ip::udp::v4() );
    boost::asio::ip::udp::endpoint remote( boost::asio::ip::make_address( public_dns_server ), http_default_port );
    socket.connect( remote );

    auto local_endpoint = socket.local_endpoint();
    return local_endpoint.address().to_string();
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to get local IP: {}", e.what() );
    return std::unexpected( net_errc::network_error );
  }
}

result< std::string > upnp::send_soap_request( const std::string& control_url,
                                               const std::string& action,
                                               const std::string& service_type,
                                               const std::string& body )
{
  try
  {
    if( control_url.empty() || action.empty() || service_type.empty() || body.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid SOAP request parameters" );
      return std::unexpected( net_errc::invalid_parameters );
    }

    // Parse control URL
    auto url_result = boost::urls::parse_uri( control_url );
    if( !url_result )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid control URL format: {}", control_url );
      return std::unexpected( net_errc::upnp_invalid_url );
    }

    const boost::urls::url_view& url = *url_result;
    std::string host                 = url.host();
    std::string path                 = url.path().empty() ? "/" : std::string( url.path() );

    if( host.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Empty host in control URL: {}", control_url );
      return std::unexpected( net_errc::upnp_invalid_url );
    }

    std::uint16_t port = url.has_port() ? url.port_number() : http_default_port;

    // Connect to gateway
    boost::asio::ip::tcp::resolver resolver( _io_context );
    boost::asio::ip::tcp::socket socket( _io_context );

    boost::system::error_code ec;
    auto endpoints = resolver.resolve( host, std::to_string( port ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to resolve SOAP host {}: {}", host, ec.message() );
      return std::unexpected( net_errc::upnp_soap_request_failed );
    }

    boost::asio::connect( socket, endpoints, ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(),
                 "Failed to connect to SOAP endpoint {}:{}: {}",
                 host,
                 port,
                 ec.message() );
      return std::unexpected( net_errc::upnp_soap_request_failed );
    }

    // Build SOAP HTTP request with dynamic service type
    std::string request = std::format( "POST {} HTTP/1.1\r\n"
                                       "Host: {}:{}\r\n"
                                       "Content-Type: text/xml; charset=\"utf-8\"\r\n"
                                       "Content-Length: {}\r\n"
                                       "SOAPAction: \"{}#{}\"\r\n"
                                       "User-Agent: Respublica/1.0\r\n"
                                       "Connection: close\r\n"
                                       "\r\n"
                                       "{}",
                                       path,
                                       host,
                                       port,
                                       body.length(),
                                       service_type,
                                       action,
                                       body );
    boost::asio::write( socket, boost::asio::buffer( request ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to send SOAP request: {}", ec.message() );
      return std::unexpected( net_errc::upnp_soap_request_failed );
    }

    // Read response
    boost::asio::streambuf response_buf;

    // Read until end of stream
    while( boost::asio::read( socket, response_buf, boost::asio::transfer_at_least( 1 ), ec ) )
      ;

    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "SOAP request read error: {}", ec.message() );
      return std::unexpected( net_errc::upnp_soap_request_failed );
    }

    std::string response( boost::asio::buffers_begin( response_buf.data() ),
                          boost::asio::buffers_end( response_buf.data() ) );

    if( response.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Empty SOAP response received" );
      return std::unexpected( net_errc::upnp_soap_request_failed );
    }

    // Check for HTTP 200 OK
    if( response.find( "HTTP/1.1 200 OK" ) == std::string::npos
        && response.find( "HTTP/1.0 200 OK" ) == std::string::npos )
    {
      LOG_ERROR( respublica::log::instance(), "SOAP request failed with response: {}", response.substr( 0, 200 ) );
      return std::unexpected( net_errc::upnp_soap_request_failed );
    }

    return response;
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "SOAP request exception: {}", e.what() );
    return std::unexpected( net_errc::upnp_soap_request_failed );
  }
}

} // namespace respublica::net