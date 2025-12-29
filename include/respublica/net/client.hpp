#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <openssl/x509.h>

#include <respublica/net/message.hpp>
#include <respublica/net/peer.hpp>
#include <respublica/net/session.hpp>

namespace respublica::net {

class upnp;

// Global message handler type (includes peer pointer)
using global_message_handler = std::function< void( std::shared_ptr< peer >, std::span< const std::byte > ) >;

class client final
{
public:
  // Reconnection policy configuration
  static constexpr int default_max_reconnect_attempts = 5;
  static constexpr std::chrono::seconds default_initial_backoff{ 1 };
  static constexpr std::chrono::seconds default_max_backoff{ 60 };

  client( boost::asio::io_context& io_context,
          std::uint16_t port,
          std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints,
          const std::filesystem::path& cert_file,
          const std::filesystem::path& key_file,
          int max_reconnect_attempts = default_max_reconnect_attempts );
  client( client&& ) noexcept            = default;
  client& operator=( client&& ) noexcept = default;
  ~client();

  client( const client& )            = delete;
  client& operator=( const client& ) = delete;

  // Broadcast message to all connected peers
  template< typename T >
  void broadcast( const T& message );

  // Send a message to a peer
  template< typename T >
  std::error_code send( const peer_id& peer, const T& message );

  // Register global message handler for all peers
  template< typename T >
  void on_receive( std::function< void( std::shared_ptr< peer >, const T& ) > handler );

private:
  void do_accept();
  void do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints );
  void setup_upnp( std::uint16_t port );
  bool generate_certificate( const std::string& cert_path, const std::string& key_path );
  void register_global_handlers( const std::shared_ptr< peer >& p );
  void register_handler_on_peer( const std::shared_ptr< peer >& p, message_type_id type_id );
  void on_handshake_complete( std::shared_ptr< peer > p, X509* peer_cert );
  void on_session_disconnect( std::shared_ptr< peer > p, std::error_code ec );
  bool should_reconnect( std::error_code ec, const std::shared_ptr< peer >& p ) const;
  void schedule_reconnect( std::shared_ptr< peer > p );
  void attempt_reconnect( std::shared_ptr< peer > p );
  std::chrono::seconds calculate_backoff( int attempt ) const;

  std::reference_wrapper< boost::asio::io_context > _ioc;
  boost::asio::strand< boost::asio::io_context::executor_type > _strand;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _context;
  std::unordered_map< peer_id, std::shared_ptr< peer > > _peers;
  std::vector< std::shared_ptr< peer > > _connecting_peers;
  std::unique_ptr< upnp > _upnp;
  std::unordered_map< message_type_id, global_message_handler > _global_handlers;
  std::filesystem::path _private_key_path;
  int _max_reconnect_attempts;
};

template< typename T >
void client::broadcast( const T& message )
{
  // Execute on strand to ensure thread-safe access to _peers map
  boost::asio::post( _strand,
                     [ this, message ]()
                     {
                       for( auto& [ id, p ]: _peers )
                       {
                         if( p->state() == peer_state::ready )
                         {
                           p->session()->send( message );
                         }
                       }
                     } );
}

template< typename T >
std::error_code client::send( const peer_id& id, const T& message )
{
  // Optimization: if already on strand, execute directly to avoid blocking
  if( _strand.running_in_this_thread() )
  {
    auto it = _peers.find( id );
    if( it == _peers.end() )
    {
      return net_errc::unknown_peer;
    }

    if( it->second->state() != peer_state::ready )
    {
      return net_errc::peer_not_ready;
    }

    return it->second->session()->send( message );
  }

  // Not on strand - use promise/future to make send() synchronous while maintaining thread safety
  auto promise                          = std::make_shared< std::promise< std::error_code > >();
  std::future< std::error_code > future = promise->get_future();

  // Execute on strand to ensure thread-safe access to _peers map
  boost::asio::post( _strand,
                     [ this, id, message, promise ]()
                     {
                       auto it = _peers.find( id );
                       if( it == _peers.end() )
                       {
                         promise->set_value( net_errc::unknown_peer );
                         return;
                       }

                       if( it->second->state() != peer_state::ready )
                       {
                         promise->set_value( net_errc::peer_not_ready );
                         return;
                       }

                       std::error_code ec = it->second->session()->send( message );
                       promise->set_value( ec );
                     } );

  // Block until strand executes and returns result
  return future.get();
}

template< typename T >
void client::on_receive( std::function< void( std::shared_ptr< peer >, const T& ) > handler )
{
  constexpr message_type_id type_id = get_message_type_id< T >();

  // Execute on strand to ensure thread-safe access to _global_handlers and _peers
  boost::asio::post( _strand,
                     [ this, type_id, handler ]()
                     {
                       // Store global handler
                       _global_handlers[ type_id ] =
                         [ handler ]( std::shared_ptr< peer > p, std::span< const std::byte > data )
                       {
                         auto result = deserialize_message< T >( data );
                         if( result )
                         {
                           handler( p, *result );
                         }
                       };

                       // Apply to existing peers
                       for( auto& [ id, p ]: _peers )
                       {
                         register_handler_on_peer( p, type_id );
                       }
                     } );
}

} // namespace respublica::net
