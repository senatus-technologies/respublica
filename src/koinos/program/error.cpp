#include <koinos/program/error.hpp>

#include <utility>

namespace koinos::program {

const char* program_category::name() const noexcept
{
  return "program";
}

std::string program_category::message( int condition ) const noexcept
{
  using namespace std::string_literals;
  switch( static_cast< program_errc >( condition ) )
  {
    case program_errc::ok:
      return "ok"s;
    case program_errc::failure:
      return "failure"s;
  }
  std::unreachable();
}

std::error_code make_error_code( program_errc e )
{
  static program_category category;
  return std::error_code( static_cast< int >( e ), category );
}

} // namespace koinos::program
