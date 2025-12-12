#include <respublica/net/upnp.hpp>

#include <chrono>
#include <regex>
#include <sstream>

namespace respublica::net {

constexpr const char* UPNP_MULTICAST_ADDR   = "239.255.255.250";
constexpr std::uint16_t UPNP_MULTICAST_PORT = 1'900;
constexpr auto UPNP_DISCOVERY_TIMEOUT       = std::chrono::seconds( 3 );

// SSDP discovery message
constexpr const char* SSDP_DISCOVER_MSG = "M-SEARCH * HTTP/1.1\r\n"
                                          "HOST: 239.255.255.250:1900\r\n"
                                          "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
                                          "MAN: \"ssdp:discover\"\r\n"
                                          "MX: 3\r\n"
                                          "\r\n";

upnp::upnp( boost::asio::io_context& io_context ):
    _io_context( io_context )
{
  LOG_INFO( respublica::log::instance(), "Initializing UPnP manager" );
  discover_gateway();
}

upnp::~upnp()
{
  if( _mapped_port > 0 )
  {
    LOG_INFO( respublica::log::instance(), "Cleaning up UPnP port mapping for port {}", _mapped_port );
    remove_port_mapping( _mapped_port, _protocol );
  }
}

void upnp::discover_gateway()
{
  try
  {
    LOG_INFO( respublica::log::instance(), "Discovering UPnP gateway via SSDP" );

    boost::asio::ip::udp::socket socket( _io_context );
    socket.open( boost::asio::ip::udp::v4() );
    socket.set_option( boost::asio::socket_base::broadcast( true ) );

    boost::asio::ip::udp::endpoint multicast_endpoint( boost::asio::ip::make_address( UPNP_MULTICAST_ADDR ),
                                                       UPNP_MULTICAST_PORT );

    socket.send_to( boost::asio::buffer( SSDP_DISCOVER_MSG, std::strlen( SSDP_DISCOVER_MSG ) ), multicast_endpoint );

    socket.non_blocking( true );

    std::array< char, 2'048 > recv_buffer;
    boost::asio::ip::udp::endpoint sender_endpoint;

    auto start_time = std::chrono::steady_clock::now();
    bool found      = false;

    while( !found && ( std::chrono::steady_clock::now() - start_time ) < UPNP_DISCOVERY_TIMEOUT )
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
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
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

    std::uint16_t port = port_str.empty() ? 80 : static_cast< std::uint16_t >( std::stoi( port_str ) );

    // Fetch device description XML
    std::string xml_response;
    if( !http_get( host, port, path, xml_response ) )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to fetch device description XML" );
      return std::nullopt;
    }

    // Parse control URL from XML
    auto control_url = parse_control_url_from_xml( xml_response );
    if( !control_url )
    {
      LOG_WARNING( respublica::log::instance(), "Could not parse control URL from device description" );
      return std::nullopt;
    }

    // Make control URL absolute if it's relative
    if( control_url->front() == '/' )
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
  // Look for WANIPConnection or WANPPPConnection service
  std::vector< std::string > service_types = { "WANIPConnection", "WANPPPConnection" };

  for( const auto& service_type: service_types )
  {
    std::string pattern = "<serviceType>urn:schemas-upnp-org:service:" + service_type + ":1</serviceType>";
    auto pos            = xml.find( pattern );

    if( pos != std::string::npos )
    {
      // Find controlURL after this service type
      auto control_start = xml.find( "<controlURL>", pos );
      if( control_start != std::string::npos )
      {
        control_start    += 12; // length of "<controlURL>"
        auto control_end  = xml.find( "</controlURL>", control_start );

        if( control_end != std::string::npos )
        {
          std::string url = xml.substr( control_start, control_end - control_start );
          LOG_INFO( respublica::log::instance(), "Found control URL: {}", url );
          return url;
        }
      }
    }
  }

  return std::nullopt;
}

bool upnp::http_get( const std::string& host, std::uint16_t port, const std::string& path, std::string& response )
{
  try
  {
    boost::asio::ip::tcp::resolver resolver( _io_context );
    boost::asio::ip::tcp::socket socket( _io_context );

    auto endpoints = resolver.resolve( host, std::to_string( port ) );
    boost::asio::connect( socket, endpoints );

    // Send HTTP GET request
    std::ostringstream request_stream;
    request_stream << "GET " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << "\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "\r\n";

    std::string request = request_stream.str();
    boost::asio::write( socket, boost::asio::buffer( request ) );

    // Read response
    boost::asio::streambuf response_buf;
    boost::asio::read_until( socket, response_buf, "\r\n\r\n" );

    // Read remaining data
    boost::system::error_code ec;
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
  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n";
  body << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
  body << "<s:Body>\r\n";
  body << "<u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n";
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
  if( !send_soap_request( *_control_url, "AddPortMapping", body.str(), response ) )
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

  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n";
  body << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
  body << "<s:Body>\r\n";
  body << "<u:DeletePortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n";
  body << "<NewRemoteHost></NewRemoteHost>\r\n";
  body << "<NewExternalPort>" << external_port << "</NewExternalPort>\r\n";
  body << "<NewProtocol>" << protocol << "</NewProtocol>\r\n";
  body << "</u:DeletePortMapping>\r\n";
  body << "</s:Body>\r\n";
  body << "</s:Envelope>\r\n";

  std::string response;
  send_soap_request( *_control_url, "DeletePortMapping", body.str(), response );

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

  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n";
  body << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
  body << "<s:Body>\r\n";
  body << "<u:GetExternalIPAddress xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n";
  body << "</u:GetExternalIPAddress>\r\n";
  body << "</s:Body>\r\n";
  body << "</s:Envelope>\r\n";

  std::string response;
  if( !send_soap_request( *_control_url, "GetExternalIPAddress", body.str(), response ) )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to get external IP via SOAP" );
    return std::nullopt;
  }

  // Parse IP from response
  auto ip_start = response.find( "<NewExternalIPAddress>" );
  if( ip_start != std::string::npos )
  {
    ip_start    += 22; // length of tag
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
    boost::asio::ip::udp::socket socket( _io_context );
    socket.open( boost::asio::ip::udp::v4() );
    boost::asio::ip::udp::endpoint remote( boost::asio::ip::make_address( "8.8.8.8" ), 80 );
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
                              const std::string& body,
                              std::string& response )
{
  try
  {
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

    std::uint16_t port = port_str.empty() ? 80 : static_cast< std::uint16_t >( std::stoi( port_str ) );

    // Connect to gateway
    boost::asio::ip::tcp::resolver resolver( _io_context );
    boost::asio::ip::tcp::socket socket( _io_context );

    auto endpoints = resolver.resolve( host, std::to_string( port ) );
    boost::asio::connect( socket, endpoints );

    // Build SOAP HTTP request
    std::ostringstream request_stream;
    request_stream << "POST " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << ":" << port << "\r\n";
    request_stream << "Content-Type: text/xml; charset=\"utf-8\"\r\n";
    request_stream << "Content-Length: " << body.length() << "\r\n";
    request_stream << "SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#" << action << "\"\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "\r\n";
    request_stream << body;

    std::string request = request_stream.str();
    boost::asio::write( socket, boost::asio::buffer( request ) );

    // Read response
    boost::asio::streambuf response_buf;
    boost::system::error_code ec;

    // Read until end of stream
    while( boost::asio::read( socket, response_buf, boost::asio::transfer_at_least( 1 ), ec ) )
      ;

    if( ec && ec != boost::asio::error::eof )
    {
      LOG_ERROR( respublica::log::instance(), "SOAP request error: {}", ec.message() );
      return false;
    }

    std::ostringstream ss;
    ss << &response_buf;
    response = ss.str();

    // Check for HTTP 200 OK
    if( response.find( "HTTP/1.1 200 OK" ) == std::string::npos
        && response.find( "HTTP/1.0 200 OK" ) == std::string::npos )
    {
      LOG_ERROR( respublica::log::instance(), "SOAP request failed: {}", response.substr( 0, 200 ) );
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
