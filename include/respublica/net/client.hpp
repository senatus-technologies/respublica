#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

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
  client( boost::asio::io_context& io_context,
          std::uint16_t port,
          std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints,
          const std::filesystem::path& cert_file,
          const std::filesystem::path& key_file );
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
  std::string get_password() const;
  void setup_upnp( std::uint16_t port );
  bool generate_certificate( const std::string& cert_path, const std::string& key_path );
  void register_global_handlers( std::shared_ptr< session > sess );

  template< typename T >
  void register_handler_on_peer( std::shared_ptr< peer > p,
                                 std::function< void( std::shared_ptr< peer >, const T& ) > handler );

  std::reference_wrapper< boost::asio::io_context > _ioc;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _context;
  std::vector< std::shared_ptr< peer > > _peers;
  std::unique_ptr< upnp > _upnp;
  std::unordered_map< message_type_id, global_message_handler > _global_handlers;
  std::filesystem::path _private_key_path;
};

template< typename T >
void client::broadcast( const T& message )
{
  for( auto& p: _peers )
    p->session()->send( message );
}

template< typename T >
std::error_code client::send( const peer_id& id, const T& message )
{
  for( auto& p: _peers )
    if( p->id() == id )
      return p->session()->send( message );

  return net_errc::unknown_peer;
}

template< typename T >
void client::on_receive( std::function< void( std::shared_ptr< peer >, const T& ) > handler )
{
  constexpr message_type_id type_id = get_message_type_id< T >();

  // Store global handler
  _global_handlers[ type_id ] = [ handler ]( std::shared_ptr< peer > p, std::span< const std::byte > data )
  {
    auto result = deserialize_message< T >( data );
    if( result )
    {
      handler( p, *result );
    }
  };

  // Apply to existing peers
  for( auto& p: _peers )
  {
    register_handler_on_peer< T >( p, handler );
  }
}

template< typename T >
void client::register_handler_on_peer( std::shared_ptr< peer > p,
                                       std::function< void( std::shared_ptr< peer >, const T& ) > handler )
{
  p->session()->on_receive< T >(
    [ handler, p ]( const T& msg )
    {
      handler( p, msg );
    } );
}

} // namespace respublica::net
