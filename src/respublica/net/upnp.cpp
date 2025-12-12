#include <respublica/log.hpp>
#include <respublica/net/upnp.hpp>

#include <chrono>
#include <regex>
#include <sstream>

#include <boost/asio.hpp>

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
          _gateway_url = location;
          _control_url = get_control_url( *location );
          found        = true;
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

std::optional< std::string > upnp::parse_location( const std::string& response )
{
  std::istringstream stream( response );
  std::string line;

  while( std::getline( stream, line ) )
  {
    if( line.find( "LOCATION:" ) == 0 || line.find( "Location:" ) == 0 )
    {
      auto pos = line.find( ':' );
      if( pos != std::string::npos )
      {
        std::string location = line.substr( pos + 1 );
        location.erase( 0, location.find_first_not_of( " \t\r\n" ) );
        location.erase( location.find_last_not_of( " \t\r\n" ) + 1 );
        return location;
      }
    }
  }

  return std::nullopt;
}

std::optional< std::string > upnp::get_control_url( const std::string& location )
{
  try
  {
    // Parse URL to extract host and path
    std::regex url_regex( R"(^(https?://)([^:/]+)(?::(\d+))?(/.*)?$)" );
    std::smatch matches;

    if( !std::regex_match( location, matches, url_regex ) )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid gateway URL format: {}", location );
      return std::nullopt;
    }

    std::string protocol = matches[ 1 ].str();
    std::string host     = matches[ 2 ].str();
    std::string port_str = matches[ 3 ].str();
    std::string path     = matches[ 4 ].str();

    std::uint16_t port = port_str.empty() ? http_default_port : static_cast< std::uint16_t >( std::stoi( port_str ) );

    // Fetch device description XML
    std::string xml_response;
    if( !http_get( host, port, path, xml_response ) )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to fetch device description XML" );
      return std::nullopt;
    }

    // Extract XML body from HTTP response
    constexpr std::size_t HTTP_HEADER_SEPARATOR_LENGTH = 4;
    auto body_start                                    = xml_response.find( "\r\n\r\n" );
    if( body_start != std::string::npos )
    {
      xml_response = xml_response.substr( body_start + HTTP_HEADER_SEPARATOR_LENGTH );
    }

    // Parse control URL from XML
    auto control_url = parse_control_url_from_xml( xml_response );
    if( !control_url )
    {
      LOG_WARNING( respublica::log::instance(), "Could not parse control URL from device description" );
      return std::nullopt;
    }

    // Make control URL absolute if it's relative
    if( !control_url->empty() && control_url->front() == '/' )
    {
      return protocol + host + ":" + std::to_string( port ) + *control_url;
    }

    return control_url;
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "Error getting control URL: {}", e.what() );
    return std::nullopt;
  }
}

std::optional< std::string > upnp::parse_control_url_from_xml( const std::string& xml )
{
  if( xml.empty() )
  {
    LOG_ERROR( respublica::log::instance(), "Empty XML device description" );
    return std::nullopt;
  }

  xmlDocPtr doc = xmlReadMemory( xml.c_str(),
                                 static_cast< int >( xml.length() ),
                                 nullptr,
                                 nullptr,
                                 XML_PARSE_NOWARNING | XML_PARSE_NOERROR | XML_PARSE_RECOVER );
  if( !doc )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to parse XML device description" );
    return std::nullopt;
  }

  xmlXPathContextPtr xpath_ctx = xmlXPathNewContext( doc );
  if( !xpath_ctx )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to create XPath context" );
    xmlFreeDoc( doc );
    return std::nullopt;
  }

  std::optional< std::string > control_url;
  std::vector< std::string > service_types = { "WANIPConnection", "WANPPPConnection" };

  for( const auto& service_type: service_types )
  {
    // XPath to find service with matching type (ignoring namespaces)
    std::string xpath =
      "//*[local-name()='serviceType' and starts-with(text(), 'urn:schemas-upnp-org:service:" + service_type + ":')]";
    xmlXPathObjectPtr xpath_obj =
      xmlXPathEvalExpression( reinterpret_cast< const xmlChar* >( xpath.c_str() ), xpath_ctx );

    if( !xpath_obj )
    {
      LOG_WARNING( respublica::log::instance(), "XPath evaluation failed for service type: {}", service_type );
      continue;
    }

    if( xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0 )
    {
      xmlNodePtr service_type_node = xpath_obj->nodesetval->nodeTab[ 0 ];
      xmlChar* service_type_text   = xmlNodeGetContent( service_type_node );

      if( service_type_text )
      {
        _service_type = std::string( reinterpret_cast< const char* >( service_type_text ) );
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
                && xmlStrcmp( child->name, reinterpret_cast< const xmlChar* >( "controlURL" ) ) == 0 )
            {
              xmlChar* url_text = xmlNodeGetContent( child );
              if( url_text )
              {
                std::string url_str( reinterpret_cast< const char* >( url_text ) );
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
  }

  return control_url;
}

bool upnp::http_get( const std::string& host, std::uint16_t port, const std::string& path, std::string& response )
{
  try
  {
    if( host.empty() || path.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid HTTP GET parameters: host or path is empty" );
      return false;
    }

    boost::asio::ip::tcp::resolver resolver( _io_context );
    boost::asio::ip::tcp::socket socket( _io_context );

    boost::system::error_code ec;
    auto endpoints = resolver.resolve( host, std::to_string( port ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to resolve host {}: {}", host, ec.message() );
      return false;
    }

    boost::asio::connect( socket, endpoints, ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to connect to {}:{}: {}", host, port, ec.message() );
      return false;
    }

    // Send HTTP GET request
    std::ostringstream request_stream;
    request_stream << "GET " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << "\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "User-Agent: Respublica/1.0\r\n";
    request_stream << "\r\n";

    std::string request = request_stream.str();
    boost::asio::write( socket, boost::asio::buffer( request ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to send HTTP request: {}", ec.message() );
      return false;
    }

    // Read response with timeout handling
    boost::asio::streambuf response_buf;
    boost::asio::read_until( socket, response_buf, "\r\n\r\n", ec );
    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to read HTTP headers: {}", ec.message() );
      return false;
    }

    // Read remaining data
    while( boost::asio::read( socket, response_buf, boost::asio::transfer_at_least( 1 ), ec ) )
      ;

    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "HTTP GET error: {}", ec.message() );
      return false;
    }

    std::ostringstream ss;
    ss << &response_buf;
    response = ss.str();

    // Validate we got some response
    if( response.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Empty HTTP response received" );
      return false;
    }

    // Check for HTTP success
    if( response.find( "HTTP/1." ) != 0 )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid HTTP response format" );
      return false;
    }

    return true;
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "HTTP GET exception: {}", e.what() );
    return false;
  }
}

std::optional< std::string >
upnp::add_port_mapping( std::uint16_t internal_port, std::uint16_t external_port, const std::string& protocol )
{
  if( !_control_url )
  {
    LOG_WARNING( respublica::log::instance(), "Cannot add port mapping: No UPnP gateway found" );
    return std::nullopt;
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
    return std::nullopt;
  }

  // Build SOAP request
  std::string service_type_urn = _service_type ? *_service_type : "urn:schemas-upnp-org:service:WANIPConnection:1";

  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n";
  body << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
  body << "<s:Body>\r\n";
  body << "<u:AddPortMapping xmlns:u=\"" << service_type_urn << "\">\r\n";
  body << "<NewRemoteHost></NewRemoteHost>\r\n";
  body << "<NewExternalPort>" << external_port << "</NewExternalPort>\r\n";
  body << "<NewProtocol>" << protocol << "</NewProtocol>\r\n";
  body << "<NewInternalPort>" << internal_port << "</NewInternalPort>\r\n";
  body << "<NewInternalClient>" << *local_ip << "</NewInternalClient>\r\n";
  body << "<NewEnabled>1</NewEnabled>\r\n";
  body << "<NewPortMappingDescription>Respublica P2P</NewPortMappingDescription>\r\n";
  body << "<NewLeaseDuration>0</NewLeaseDuration>\r\n";
  body << "</u:AddPortMapping>\r\n";
  body << "</s:Body>\r\n";
  body << "</s:Envelope>\r\n";

  std::string response;
  if( !send_soap_request( *_control_url, "AddPortMapping", service_type_urn, body.str(), response ) )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to add port mapping via SOAP" );
    return std::nullopt;
  }

  // Store for cleanup
  _mapped_port = external_port;
  _protocol    = protocol;

  LOG_INFO( respublica::log::instance(), "UPnP port mapping added successfully" );
  return get_external_ip();
}

void upnp::remove_port_mapping( std::uint16_t external_port, const std::string& protocol )
{
  if( !_control_url )
  {
    return;
  }

  LOG_INFO( respublica::log::instance(), "Removing UPnP port mapping for port {} ({})", external_port, protocol );

  std::string service_type_urn = _service_type ? *_service_type : "urn:schemas-upnp-org:service:WANIPConnection:1";

  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n";
  body << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
  body << "<s:Body>\r\n";
  body << "<u:DeletePortMapping xmlns:u=\"" << service_type_urn << "\">\r\n";
  body << "<NewRemoteHost></NewRemoteHost>\r\n";
  body << "<NewExternalPort>" << external_port << "</NewExternalPort>\r\n";
  body << "<NewProtocol>" << protocol << "</NewProtocol>\r\n";
  body << "</u:DeletePortMapping>\r\n";
  body << "</s:Body>\r\n";
  body << "</s:Envelope>\r\n";

  std::string response;
  send_soap_request( *_control_url, "DeletePortMapping", service_type_urn, body.str(), response );

  _mapped_port = 0;
}

std::optional< std::string > upnp::get_external_ip()
{
  if( _external_ip )
  {
    return _external_ip;
  }

  if( !_control_url )
  {
    return std::nullopt;
  }

  LOG_INFO( respublica::log::instance(), "Querying external IP via UPnP" );

  std::string service_type_urn = _service_type ? *_service_type : "urn:schemas-upnp-org:service:WANIPConnection:1";

  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n";
  body << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
  body << "<s:Body>\r\n";
  body << "<u:GetExternalIPAddress xmlns:u=\"" << service_type_urn << "\">\r\n";
  body << "</u:GetExternalIPAddress>\r\n";
  body << "</s:Body>\r\n";
  body << "</s:Envelope>\r\n";

  std::string response;
  if( !send_soap_request( *_control_url, "GetExternalIPAddress", service_type_urn, body.str(), response ) )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to get external IP via SOAP" );
    return std::nullopt;
  }

  // Parse IP from response
  constexpr std::size_t new_external_ip_tag_length = 22;
  auto ip_start                                    = response.find( "<NewExternalIPAddress>" );
  if( ip_start != std::string::npos )
  {
    ip_start    += new_external_ip_tag_length;
    auto ip_end  = response.find( "</NewExternalIPAddress>", ip_start );

    if( ip_end != std::string::npos )
    {
      _external_ip = response.substr( ip_start, ip_end - ip_start );
      LOG_INFO( respublica::log::instance(), "External IP: {}", *_external_ip );
      return _external_ip;
    }
  }

  return std::nullopt;
}

std::optional< std::string > upnp::get_local_ip()
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
    return std::nullopt;
  }
}

bool upnp::send_soap_request( const std::string& control_url,
                              const std::string& action,
                              const std::string& service_type,
                              const std::string& body,
                              std::string& response )
{
  try
  {
    if( control_url.empty() || action.empty() || service_type.empty() || body.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid SOAP request parameters" );
      return false;
    }

    // Parse control URL
    std::regex url_regex( R"(^(https?://)([^:/]+)(?::(\d+))?(/.*)?$)" );
    std::smatch matches;

    if( !std::regex_match( control_url, matches, url_regex ) )
    {
      LOG_ERROR( respublica::log::instance(), "Invalid control URL format: {}", control_url );
      return false;
    }

    std::string host     = matches[ 2 ].str();
    std::string port_str = matches[ 3 ].str();
    std::string path     = matches[ 4 ].str();

    if( host.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Empty host in control URL: {}", control_url );
      return false;
    }

    if( path.empty() )
    {
      path = "/";
    }

    std::uint16_t port = port_str.empty() ? http_default_port : static_cast< std::uint16_t >( std::stoi( port_str ) );

    // Connect to gateway
    boost::asio::ip::tcp::resolver resolver( _io_context );
    boost::asio::ip::tcp::socket socket( _io_context );

    boost::system::error_code ec;
    auto endpoints = resolver.resolve( host, std::to_string( port ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to resolve SOAP host {}: {}", host, ec.message() );
      return false;
    }

    boost::asio::connect( socket, endpoints, ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(),
                 "Failed to connect to SOAP endpoint {}:{}: {}",
                 host,
                 port,
                 ec.message() );
      return false;
    }

    // Build SOAP HTTP request with dynamic service type
    std::ostringstream request_stream;
    request_stream << "POST " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << ":" << port << "\r\n";
    request_stream << "Content-Type: text/xml; charset=\"utf-8\"\r\n";
    request_stream << "Content-Length: " << body.length() << "\r\n";
    request_stream << "SOAPAction: \"" << service_type << "#" << action << "\"\r\n";
    request_stream << "User-Agent: Respublica/1.0\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "\r\n";
    request_stream << body;

    std::string request = request_stream.str();
    boost::asio::write( socket, boost::asio::buffer( request ), ec );
    if( ec )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to send SOAP request: {}", ec.message() );
      return false;
    }

    // Read response
    boost::asio::streambuf response_buf;

    // Read until end of stream
    while( boost::asio::read( socket, response_buf, boost::asio::transfer_at_least( 1 ), ec ) )
      ;

    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "SOAP request read error: {}", ec.message() );
      return false;
    }

    std::ostringstream ss;
    ss << &response_buf;
    response = ss.str();

    if( response.empty() )
    {
      LOG_ERROR( respublica::log::instance(), "Empty SOAP response received" );
      return false;
    }

    // Check for HTTP 200 OK
    if( response.find( "HTTP/1.1 200 OK" ) == std::string::npos
        && response.find( "HTTP/1.0 200 OK" ) == std::string::npos )
    {
      LOG_ERROR( respublica::log::instance(), "SOAP request failed with response: {}", response.substr( 0, 200 ) );
      return false;
    }

    return true;
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "SOAP request exception: {}", e.what() );
    return false;
  }
}

} // namespace respublica::net
