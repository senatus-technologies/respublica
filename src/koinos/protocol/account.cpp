#include <koinos/protocol/account.hpp>

namespace koinos::protocol {

constexpr account system_account( std::string_view str ) noexcept
{
  account a{};
  std::size_t length = std::min( str.length(), a.size() );
  for( std::size_t i = 0; i < length; ++i )
    a[ i ] = static_cast< std::byte >( str[ i ] );
  return a;
}

} // namespace koinos::protocol
