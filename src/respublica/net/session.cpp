#include <respublica/net/session.hpp>

#include <functional>

namespace respublica::net {

session::session( boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket ):
    _socket( std::move( socket ) )
{
  _socket.set_verify_mode( boost::asio::ssl::verify_peer );
  _socket.set_verify_callback(
    std::bind( &session::verify_certificate, this, std::placeholders::_1, std::placeholders::_2 ) );

  // Pre-allocate receive buffer for header
  _receive_buffer.resize( message_header_size );
}

void session::start()
{
  do_handshake( boost::asio::ssl::stream_base::server,
                [ this ]()
                {
                  do_read_header();
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
                        do_read_header();
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

  constexpr std::size_t cert_name_buffer_size = 256;

  // Get certificate subject and issuer information
  std::array< char, cert_name_buffer_size > subject_name{};
  std::array< char, cert_name_buffer_size > issuer_name{};
  X509_NAME_oneline( X509_get_subject_name( cert ), subject_name.data(), cert_name_buffer_size );
  X509_NAME_oneline( X509_get_issuer_name( cert ), issuer_name.data(), cert_name_buffer_size );

  // Get verification depth (0 = peer cert, higher = CA certs)
  LOG_DEBUG( respublica::log::instance(),
             "Verifying certificate at depth {}",
             X509_STORE_CTX_get_error_depth( ctx.native_handle() ) );
  LOG_DEBUG( respublica::log::instance(), "  Subject: {}", subject_name.data() );
  LOG_DEBUG( respublica::log::instance(), "  Issuer:  {}", issuer_name.data() );

  if( !preverified )
  {
    int error = X509_STORE_CTX_get_error( ctx.native_handle() );

    // Allow self-signed certificates
    if( error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT || error == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN )
    {
      LOG_WARNING( respublica::log::instance(),
                   "Accepting self-signed certificate: {}",
                   X509_verify_cert_error_string( error ) );
      return true;
    }

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
                            const std::function< void( void ) >& then )
{
  auto self( shared_from_this() );
  _socket.async_handshake( handshake_type,
                           [ this, then, self ]( const boost::system::error_code& error )
                           {
                             if( !error )
                             {
                               // Invoke handshake completion callback with peer certificate
                               if( _handshake_callback )
                               {
                                 // Get peer certificate
                                 X509* peer_cert = SSL_get_peer_certificate( _socket.native_handle() );
                                 _handshake_callback( peer_cert );
                                 // Note: peer_cert is owned by SSL context, don't free it
                               }

                               then();
                             }
                             else
                             {
                               LOG_ERROR( respublica::log::instance(), "Handshake error: {}", error.message() );
                             }
                           } );
}

void session::do_read_header()
{
  auto self( shared_from_this() );

  boost::asio::async_read( _socket,
                           boost::asio::buffer( _receive_buffer.data(), message_header_size ),
                           [ this, self ]( const boost::system::error_code& ec, std::size_t /*length*/ )
                           {
                             if( !ec )
                             {
                               // Parse header
                               auto header_result = parse_header(
                                 std::span< const std::byte >( _receive_buffer.data(), message_header_size ) );

                               if( !header_result )
                               {
                                 LOG_ERROR( respublica::log::instance(),
                                            "Failed to parse message header: {}",
                                            header_result.error().message() );
                                 return;
                               }

                               LOG_DEBUG( respublica::log::instance(),
                                          "Received message header: type={}, version={}, length={}",
                                          static_cast< std::uint32_t >( header_result->type_id ),
                                          header_result->version,
                                          header_result->length );

                               // Read payload
                               do_read_payload( *header_result );
                             }
                             else if( ec != boost::asio::error::eof )
                             {
                               LOG_ERROR( respublica::log::instance(), "Read header error: {}", ec.message() );
                             }
                           } );
}

void session::do_read_payload( const message_header& header )
{
  auto self( shared_from_this() );

  // Resize buffer for payload (excluding type_id and version already in length)
  const std::size_t payload_size = header.length - sizeof( std::uint32_t ) - sizeof( std::uint16_t );

  _receive_buffer.resize( payload_size );

  boost::asio::async_read(
    _socket,
    boost::asio::buffer( _receive_buffer.data(), payload_size ),
    [ this, self, header ]( const boost::system::error_code& ec, std::size_t /*length*/ )
    {
      if( !ec )
      {
        LOG_DEBUG( respublica::log::instance(), "Received message payload: {} bytes", _receive_buffer.size() );

        // Handle complete message
        handle_message( header, _receive_buffer );

        // Continue reading next message
        _receive_buffer.resize( message_header_size );
        do_read_header();
      }
      else
      {
        LOG_ERROR( respublica::log::instance(), "Read payload error: {}", ec.message() );
      }
    } );
}

void session::handle_message( const message_header& header, std::span< const std::byte > payload )
{
  // Find handler for this message type
  auto it = _message_handlers.find( header.type_id );
  if( it != _message_handlers.end() )
  {
    // Invoke handler
    it->second( payload );
  }
  else
  {
    LOG_WARNING( respublica::log::instance(),
                 "No handler registered for message type: {}",
                 static_cast< std::uint32_t >( header.type_id ) );
  }
}

void session::enqueue_send( std::vector< std::byte > data )
{
  auto self( shared_from_this() );

  boost::asio::post( _socket.get_executor(),
                     [ this, self, data = std::move( data ) ]() mutable
                     {
                       _send_queue.push( std::move( data ) );

                       if( !_writing )
                       {
                         do_write();
                       }
                     } );
}

void session::do_write()
{
  if( _send_queue.empty() )
  {
    _writing = false;
    return;
  }

  _writing = true;
  auto self( shared_from_this() );

  const auto& front = _send_queue.front();

  boost::asio::async_write( _socket,
                            boost::asio::buffer( front.data(), front.size() ),
                            [ this, self ]( const boost::system::error_code& ec, std::size_t bytes_written )
                            {
                              if( !ec )
                              {
                                LOG_DEBUG( respublica::log::instance(), "Sent message: {} bytes", bytes_written );

                                _send_queue.pop();
                                do_write(); // Send next message
                              }
                              else
                              {
                                LOG_ERROR( respublica::log::instance(), "Write error: {}", ec.message() );
                                _writing = false;
                              }
                            } );
}

} // namespace respublica::net
