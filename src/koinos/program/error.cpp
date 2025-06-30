#include <koinos/program/error.hpp>

#include <string>
#include <utility>

namespace koinos::program {

struct _program_category final: std::error_category
{
  const char* name() const noexcept final;
  std::string message( int condition ) const noexcept final;
};

const char* _program_category::name() const noexcept
{
  return "program";
}

std::string _program_category::message( int condition ) const noexcept
{
  using namespace std::string_literals;
  switch( static_cast< program_errc >( condition ) )
  {
    case program_errc::ok:
      return "ok"s;
    case program_errc::unauthorized:
      return "unauthorized"s;
    case program_errc::invalid_instruction:
      return "invalid instruction"s;
    case program_errc::insufficient_balance:
      return "insufficient balance"s;
    case program_errc::insufficient_supply:
      return "insufficient supply"s;
    case program_errc::invalid_argument:
      return "invalid_argument"s;
    case program_errc::unexpected_object:
      return "unexpected object"s;
    case program_errc::overflow:
      return "overflow"s;
  }
  std::unreachable();
}

const std::error_category& program_category() noexcept
{
  static _program_category category;
  return category;
}

std::error_code make_error_code( program_errc e )
{
  return std::error_code( static_cast< int >( e ), program_category() );
}

} // namespace koinos::program
