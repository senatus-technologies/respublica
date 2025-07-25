#include <respublica/encode/error.hpp>

#include <utility>

namespace respublica::encode {

struct _encode_category final: std::error_category
{
  const char* name() const noexcept final;
  std::string message( int condition ) const noexcept final;
};

const char* _encode_category::name() const noexcept
{
  return "encode";
}

std::string _encode_category::message( int condition ) const noexcept
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

const std::error_category& encode_category() noexcept
{
  static _encode_category category;
  return category;
}

std::error_code make_error_code( encode_errc e )
{
  return std::error_code( static_cast< int >( e ), encode_category() );
}

} // namespace respublica::encode
