#pragma once

#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

namespace respublica::net {

class client
{
public:
  client( boost::asio::io_context& io_context,
          std::uint16_t port,
          std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints = {} );

private:
  void do_accept();
  std::string get_password() const;

  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _context;
};

} // namespace respublica::net
