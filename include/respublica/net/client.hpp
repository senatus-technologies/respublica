#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <respublica/net/message.hpp>

namespace respublica::net {

class session;
class upnp;

// Global message handler type (includes session pointer)
using global_message_handler = std::function< void( std::shared_ptr< session >, std::span< const std::byte > ) >;

class client final
{
public:
  client( const client& )            = delete;
  client( client&& )                 = delete;
  client& operator=( const client& ) = delete;
  client& operator=( client&& )      = delete;
  client( boost::asio::io_context& io_context,
          std::uint16_t port,
          std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints,
          const std::string& cert_path,
          const std::string& key_path );
  ~client();

  // Broadcast message to all connected peers
  template< typename T >
  void broadcast( const T& message );

  // Register global message handler for all sessions
  template< typename T >
  void on_receive( std::function< void( std::shared_ptr< session >, const T& ) > handler );

private:
  void do_accept();
  void do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints );
  std::string get_password() const;
  void setup_upnp( std::uint16_t port );
  bool generate_certificate( const std::string& cert_path, const std::string& key_path );
  void register_global_handlers( std::shared_ptr< session > sess );

  template< typename T >
  void register_handler_on_session( std::shared_ptr< session > sess,
                                    std::function< void( std::shared_ptr< session >, const T& ) > handler );

  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _context;
  std::vector< std::shared_ptr< session > > _sessions;
  std::unique_ptr< upnp > _upnp;
  std::unordered_map< message_type_id, global_message_handler > _global_handlers;
};

} // namespace respublica::net

// Include session.hpp for template implementations
#include <respublica/net/session.hpp>

namespace respublica::net {

// Template implementations

template< typename T >
void client::broadcast( const T& message )
{
  for( auto& sess: _sessions )
  {
    sess->send( message );
  }
}

template< typename T >
void client::on_receive( std::function< void( std::shared_ptr< session >, const T& ) > handler )
{
  const message_type_id type_id = get_message_type_id< T >();

  // Store global handler
  _global_handlers[ type_id ] = [ handler ]( std::shared_ptr< session > sess, std::span< const std::byte > data )
  {
    auto result = deserialize_message< T >( data );
    if( result )
    {
      handler( sess, *result );
    }
  };

  // Apply to existing sessions
  for( auto& sess: _sessions )
  {
    register_handler_on_session< T >( sess, handler );
  }
}

template< typename T >
void client::register_handler_on_session( std::shared_ptr< session > sess,
                                          std::function< void( std::shared_ptr< session >, const T& ) > handler )
{
  sess->on_receive< T >(
    [ handler, sess ]( const T& msg )
    {
      handler( sess, msg );
    } );
}

} // namespace respublica::net
