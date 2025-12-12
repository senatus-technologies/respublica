#include <respublica/net/session.hpp>

#include <functional>
#include <iostream>

namespace respublica::net {

session::session( boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket ):
    _socket( std::move( socket ) ),
    _stdin( _socket.get_executor(), ::dup( STDIN_FILENO ) )
{
  _socket.set_verify_mode( boost::asio::ssl::verify_peer );
  _socket.set_verify_callback(
    std::bind( &session::verify_certificate, this, std::placeholders::_1, std::placeholders::_2 ) );
}

void session::start()
{
  do_handshake( boost::asio::ssl::stream_base::server,
                [ this ]()
                {
                  do_read();
                  do_read_stdin();
                } );
}

void session::connect( const boost::asio::ip::tcp::resolver::results_type& endpoints )
{
  boost::asio::async_connect(
    _socket.lowest_layer(),
    endpoints,
    [ this ]( const boost::system::error_code& error, const boost::asio::ip::tcp::endpoint& /*endpoint*/ )
    {
      if( !error )
      {
        do_handshake( boost::asio::ssl::stream_base::client,
                      [ this ]()
                      {
                        do_read();
                        do_read_stdin();
                      } );
      }
      else
      {
        std::cerr << "Connection error: " << error.message() << std::endl;
      }
    } );
}

bool session::verify_certificate( bool preverified, boost::asio::ssl::verify_context& ctx )
{
  // The verify callback can be used to check whether the certificate that is
  // being presented is valid for the peer. For example, RFC 2818 describes
  // the steps involved in doing this for HTTPS. Consult the OpenSSL
  // documentation for more details. Note that the callback is called once
  // for each certificate in the certificate chain, starting from the root
  // certificate authority.

  // In this example we will simply print the certificate's subject name.
  char subject_name[ 256 ];
  X509* cert = X509_STORE_CTX_get_current_cert( ctx.native_handle() );
  X509_NAME_oneline( X509_get_subject_name( cert ), subject_name, 256 );

  return preverified;
}

void session::do_handshake( boost::asio::ssl::stream_base::handshake_type handshake_type,
                            std::function< void( void ) > then )
{
  auto self( shared_from_this() );
  _socket.async_handshake( handshake_type,
                           [ then, self ]( const boost::system::error_code& error )
                           {
                             if( !error )
                             {
                               then();
                             }
                             else
                             {
                               std::cerr << "Handshake error: " << error.message() << std::endl;
                             }
                           } );
}

void session::do_read()
{
  auto self( shared_from_this() );
  _socket.async_read_some( boost::asio::buffer( _data ),
                           [ this, self ]( const boost::system::error_code& ec, std::size_t length )
                           {
                             if( !ec && length > 0 )
                             {
                               std::cout.write( _data.data(), length );
                               std::cout.flush();
                               do_read();
                             }
                             else if( ec )
                             {
                               std::cerr << "Read error: " << ec.message() << std::endl;
                             }
                           } );
}

void session::do_read_stdin()
{
  auto self( shared_from_this() );
  _stdin.async_read_some( boost::asio::buffer( _stdin_data ),
                          [ this, self ]( const boost::system::error_code& ec, std::size_t length )
                          {
                            if( !ec && length > 0 )
                            {
                              boost::asio::async_write( _socket,
                                                        boost::asio::buffer( _stdin_data, length ),
                                                        [ this, self ]( const boost::system::error_code& write_ec,
                                                                        std::size_t /*write_length*/ )
                                                        {
                                                          if( !write_ec )
                                                          {
                                                            do_read_stdin();
                                                          }
                                                        } );
                            }
                            else
                            {
                              do_read_stdin();
                            }
                          } );
}

} // namespace respublica::net
