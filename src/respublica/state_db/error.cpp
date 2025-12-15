#include <respublica/state_db/error.hpp>
#include <string>
#include <system_error>
#include <utility>

namespace respublica::state_db {

struct _state_db_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "state_db";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< state_db_errc >( condition ) )
    {
      case state_db_errc::ok:
        return "ok"s;
      case state_db_errc::parent_not_complete:
        return "parent not complete"s;
      case state_db_errc::conflicting_parents:
        return "conflicting parents"s;
      case state_db_errc::not_finalized:
        return "not finalized"s;
    }
    std::unreachable();
  }
};

const std::error_category& state_db_category() noexcept
{
  static _state_db_category category;
  return category;
}

std::error_code make_error_code( state_db_errc e )
{
  return std::error_code( static_cast< int >( e ), state_db_category() );
}

} // namespace respublica::state_db
