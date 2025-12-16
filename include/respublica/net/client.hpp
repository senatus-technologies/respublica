#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

namespace respublica::net {

class session;
class upnp;

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

private:
  void do_accept();
  void do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints );
  std::string get_password() const;
  void setup_upnp( std::uint16_t port );
  bool generate_certificate( const std::string& cert_path, const std::string& key_path );

  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _context;
  std::vector< std::shared_ptr< session > > _sessions;
  std::unique_ptr< upnp > _upnp;
};

} // namespace respublica::net
