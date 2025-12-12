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
        LOG_ERROR( respublica::log::instance(), "Connection error: {}", error.message() );
      }
    } );
}

bool session::verify_certificate( bool preverified, boost::asio::ssl::verify_context& ctx )
{
  // Get the certificate being verified
  X509* cert = X509_STORE_CTX_get_current_cert( ctx.native_handle() );
  if( !cert )
  {
    LOG_ERROR( respublica::log::instance(), "Certificate verification failed: No certificate provided" );
    return false;
  }

  // Get certificate subject and issuer information
  char subject_name[ 256 ];
  char issuer_name[ 256 ];
  X509_NAME_oneline( X509_get_subject_name( cert ), subject_name, 256 );
  X509_NAME_oneline( X509_get_issuer_name( cert ), issuer_name, 256 );

  // Get verification depth (0 = peer cert, higher = CA certs)
  LOG_DEBUG( respublica::log::instance(),
             "Verifying certificate at depth {}",
             X509_STORE_CTX_get_error_depth( ctx.native_handle() ) );
  LOG_DEBUG( respublica::log::instance(), "  Subject: {}", subject_name );
  LOG_DEBUG( respublica::log::instance(), "  Issuer:  {}", issuer_name );

  if( !preverified )
  {
    int error = X509_STORE_CTX_get_error( ctx.native_handle() );
    LOG_ERROR( respublica::log::instance(),
               "Certificate verification failed: {}",
               X509_verify_cert_error_string( error ) );
    LOG_ERROR( respublica::log::instance(), "  Error code: {}", error );
    return false;
  }

  // Additional custom verification checks can be added here
  // For example, check common name, check certificate expiration, etc.

  LOG_INFO( respublica::log::instance(), "Certificate verification: OK" );
  return true;
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
                               LOG_ERROR( respublica::log::instance(), "Handshake error: {}", error.message() );
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
                               LOG_DEBUG( respublica::log::instance(), "Received {} bytes from peer", length );
                               std::cout.write( _data.data(), length );
                               std::cout.flush();
                               do_read();
                             }
                             else if( ec )
                             {
                               LOG_ERROR( respublica::log::instance(), "Read error: {}", ec.message() );
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
