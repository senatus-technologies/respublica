#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

namespace respublica::net {

class session;

class client
{
public:
  client( boost::asio::io_context& io_context,
          std::uint16_t port,
          std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints = {} );

private:
  void do_accept();
  void do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints );
  std::string get_password() const;

  boost::asio::io_context& _io_context;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _context;
  std::vector< std::shared_ptr< session > > _sessions;
};

} // namespace respublica::net
