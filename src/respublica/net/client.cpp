#include <respublica/net/client.hpp>
#include <respublica/net/session.hpp>

#include <functional>
#include <iostream>
#include <string>

namespace respublica::net {

client::client( boost::asio::io_context& io_context,
                std::uint16_t port,
                std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints ):
    _io_context( io_context ),
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

  do_accept();

  if( endpoints )
  {
    do_connect( *endpoints );
  }
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
        auto sess = std::make_shared< session >(
          boost::asio::ssl::stream< boost::asio::ip::tcp::socket >( std::move( socket ), _context ) );
        _sessions.push_back( sess );
        sess->start();
      }
      else
      {
        std::cerr << "Accept error: " << error.message() << std::endl;
      }

      do_accept();
    } );
}

void client::do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints )
{
  boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket( _io_context, _context );
  auto sess = std::make_shared< session >( std::move( socket ) );
  _sessions.push_back( sess );
  sess->connect( endpoints );
}

} // namespace respublica::net
