#include <respublica/net/client.hpp>
#include <respublica/net/session.hpp>

#include <functional>
#include <string>

namespace respublica::net {

client::client( boost::asio::io_context& io_context,
                std::uint16_t port,
                std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints ):
    _acceptor( io_context, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), port ) ),
    _context( boost::asio::ssl::context::tlsv13_server )
{
  _context.set_options( boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
                        | boost::asio::ssl::context::single_dh_use );
  //_context.set_password_callback( std::bind( &client::get_password, this ) );
  _context.use_certificate_chain_file( "cert.pem" );
  _context.use_private_key_file( "server.pem", boost::asio::ssl::context::pem );
  //_context.use_tmp_dh_file( "dh4096.pem" );

  do_accept();

  if( endpoints )
  {
    boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket( io_context, _context );
    std::make_shared< session >( std::move( socket ) )->connect( *endpoints );
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
        std::make_shared< session >(
          boost::asio::ssl::stream< boost::asio::ip::tcp::socket >( std::move( socket ), _context ) )
          ->start();
      }

      do_accept();
    } );
}

} // namespace respublica::net
