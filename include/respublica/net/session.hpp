#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <openssl/x509.h>

#include <respublica/log.hpp>
#include <respublica/net/error.hpp>
#include <respublica/net/message.hpp>

namespace respublica::net {

// Message handler type (type-erased)
using message_handler = std::function< void( std::span< const std::byte > ) >;

// Handshake completion callback (receives peer certificate)
using handshake_callback = std::function< void( X509* ) >;

class session: public std::enable_shared_from_this< session >
{
public:
  session( boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket );

  void start();
  void connect( const boost::asio::ip::tcp::resolver::results_type& endpoints );

  // Set callback to be invoked when handshake completes
  void on_handshake_complete( handshake_callback callback )
  {
    _handshake_callback = std::move( callback );
  }

  // Send typed message
  template< typename T >
  std::error_code send( const T& message )
  {
    // Serialize message
    auto payload_result = serialize_message( message );
    if( !payload_result )
      return payload_result.error();

    // Frame message (zero-copy payload)
    auto frame_result =
      frame_message( get_message_type_id< T >(), current_protocol_version, std::move( *payload_result ) );
    if( !frame_result )
      return frame_result.error();

    // Enqueue for sending
    enqueue_send( std::move( *frame_result ) );

    return net_errc::ok;
  }

  // Register message handler
  void on_receive( message_type_id type_id, message_handler handler )
  {
    _message_handlers[ type_id ] = std::move( handler );
  }

private:
  bool verify_certificate( bool preverified, boost::asio::ssl::verify_context& ctx );
  void do_handshake( boost::asio::ssl::stream_base::handshake_type handshake_type,
                     const std::function< void( void ) >& then );
  void do_read_header();
  void do_read_payload( const message_header& header );
  void do_write();
  void enqueue_send( message_frame frame );
  void handle_message( const message_header& header, std::span< const std::byte > payload );

  boost::asio::ssl::stream< boost::asio::ip::tcp::socket > _socket;
  boost::asio::strand< boost::asio::any_io_executor > _strand;

  // Message handling
  std::unordered_map< message_type_id, message_handler > _message_handlers;

  // Handshake completion callback
  handshake_callback _handshake_callback;

  // Receive buffer (for accumulating partial messages)
  std::vector< std::byte > _receive_buffer;

  // Send queue (for backpressure management)
  std::queue< message_frame > _send_queue;
};

} // namespace respublica::net
