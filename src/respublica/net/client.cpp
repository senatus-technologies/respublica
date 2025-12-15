#include <respublica/net/client.hpp>
#include <respublica/net/session.hpp>
#include <respublica/net/upnp.hpp>

#include <string>

#include <respublica/log.hpp>

namespace respublica::net {

client::client( boost::asio::io_context& io_context,
                std::uint16_t port,
                std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints,
                bool enable_upnp ):
    _acceptor( io_context, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), port ) ),
    _context( boost::asio::ssl::context::tlsv13 )
{
  // Configure context for both server and client mode
  _context.set_options( boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
                        | boost::asio::ssl::context::single_dh_use );
  _context.use_certificate_chain_file( "cert.pem" );
  _context.use_private_key_file( "server.pem", boost::asio::ssl::context::pem );
  _context.set_verify_mode( boost::asio::ssl::verify_peer );
  _context.load_verify_file( "cert.pem" );

  // Setup UPnP if enabled
  if( enable_upnp )
  {
    setup_upnp( port );
  }

  do_accept();

  if( endpoints )
  {
    do_connect( *endpoints );
  }
}

client::~client()
{
  // Destructor must be defined in the .cpp file where upnp is complete
}

std::string client::get_password() const
{
  return "test";
}

void client::do_accept()
{
  _acceptor.async_accept(
    [ this ]( const boost::system::error_code& error, boost::asio::ip::tcp::socket socket )
    {
      if( !error )
      {
        LOG_INFO( respublica::log::instance(), "Accepted incoming connection" );
        auto sess = std::make_shared< session >(
          boost::asio::ssl::stream< boost::asio::ip::tcp::socket >( std::move( socket ), _context ) );
        _sessions.push_back( sess );
        sess->start();
      }
      else
      {
        LOG_ERROR( respublica::log::instance(), "Accept error: {}", error.message() );
      }

      do_accept();
    } );
}

void client::do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints )
{
  LOG_INFO( respublica::log::instance(), "Initiating connection to remote peer" );
  boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket( _acceptor.get_executor(), _context );
  auto sess = std::make_shared< session >( std::move( socket ) );
  _sessions.push_back( sess );
  sess->connect( endpoints );
}

void client::setup_upnp( std::uint16_t port )
{
  try
  {
    auto& ctx        = static_cast< boost::asio::io_context& >( _acceptor.get_executor().context() );
    _upnp            = std::make_unique< upnp >( ctx );
    auto external_ip = _upnp->add_port_mapping( port, port, "TCP" );

    if( external_ip )
    {
      LOG_INFO( respublica::log::instance(), "UPnP port mapping successful. External IP: {}", *external_ip );
    }
    else
    {
      LOG_WARNING( respublica::log::instance(), "UPnP port mapping failed: {}", external_ip.error().message() );
    }
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "UPnP setup failed: {}", e.what() );
    _upnp.reset();
  }
}

} // namespace respublica::net
