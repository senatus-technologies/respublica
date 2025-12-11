#include <functional>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace respublica::net {

constexpr std::size_t max_message_size = 1'024;

class session: public std::enable_shared_from_this< session >
{
public:
  session( boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket );

  void start();
  void connect( const boost::asio::ip::tcp::resolver::results_type& endpoints );

private:
  bool verify_certificate( bool preverified, boost::asio::ssl::verify_context& ctx );
  void do_handshake( boost::asio::ssl::stream_base::handshake_type handshake_type, std::function< void( void ) > then );
  void do_read();
  void do_write( std::size_t length );

  boost::asio::ssl::stream< boost::asio::ip::tcp::socket > _socket;
  std::array< char, max_message_size > _data{};
};

} // namespace respublica::net
