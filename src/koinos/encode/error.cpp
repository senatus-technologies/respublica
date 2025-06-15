#include <koinos/encode/error.hpp>

#include <utility>

namespace koinos::encode {

const char* encode_category::name() const noexcept
{
  return "encode";
}

std::string encode_category::message( int condition ) const noexcept
{
  using namespace std::string_literals;
  switch( static_cast< encode_errc >( condition ) )
  {
    case encode_errc::ok:
      return "ok"s;
    case encode_errc::invalid_character:
      return "invalid character"s;
    case encode_errc::invalid_length:
      return "invalid length"s;
  }
  std::unreachable();
}

std::error_code make_error_code( encode_errc e )
{
  static encode_category category;
  return std::error_code( static_cast< int >( e ), category );
}

} // namespace koinos::encode
