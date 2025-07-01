#include <koinos/protocol/account.hpp>

namespace koinos::protocol {

constexpr auto user_account_prefix           = std::byte{ std::to_underlying( account_type::user ) };
constexpr auto program_account_prefix        = std::byte{ std::to_underlying( account_type::program ) };
constexpr auto native_program_account_prefix = std::byte{ std::to_underlying( account_type::native_program ) };

static account_type account_prefix_to_type( std::byte prefix ) noexcept
{
  switch( std::to_integer< std::uint8_t >( prefix ) )
  {
    case std::to_underlying( account_type::user ):
      return account_type::user;
    case std::to_underlying( account_type::program ):
      return account_type::program;
    case std::to_underlying( account_type::native_program ):
      return account_type::native_program;
    default:
      return account_type::invalid;
  }
  std::unreachable();
}

account::operator crypto::public_key() const noexcept
{
  return crypto::public_key( crypto::public_key_data_view( begin() + 1, end() ) );
}

bool account::user() const noexcept
{
  return at( 0 ) == user_account_prefix;
}

bool account::program() const noexcept
{
  return std::to_integer< std::uint8_t >( at( 0 ) );
}

account_type account::type() const noexcept
{
  return account_prefix_to_type( at( 0 ) );
}

account user_account( const crypto::public_key& pub_key ) noexcept
{
  account a{ user_account_prefix };
  std::ranges::copy( pub_key.bytes(), a.begin() + 1 );
  return a;
}

account user_account( const account& acc ) noexcept
{
  account a = acc;
  a.at( 0 ) = user_account_prefix;
  return a;
}

account program_account( const crypto::public_key& pub_key ) noexcept
{
  account a{ program_account_prefix };
  std::ranges::copy( pub_key.bytes(), a.begin() + 1 );
  return a;
}

account program_account( const account& acc ) noexcept
{
  account a = acc;
  a.at( 0 ) = program_account_prefix;
  return a;
}

account_view::account_view( const account& acc ) noexcept:
    std::span< const std::byte, crypto::public_key_length + 1 >( acc )
{}

account_view::account_view( const std::byte* ptr, std::size_t length ) noexcept:
    std::span< const std::byte, crypto::public_key_length + 1 >( ptr, length )
{}

account_view::operator crypto::public_key() const noexcept
{
  return crypto::public_key( crypto::public_key_data_view( begin() + 1, end() ) );
}

bool account_view::user() const noexcept
{
  return ( *this )[ 0 ] == user_account_prefix;
}

bool account_view::program() const noexcept
{
  return ( at( 0 ) & program_account_prefix ) != std::byte{ 0x00 };
}

account_type account_view::type() const noexcept
{
  return account_prefix_to_type( at( 0 ) );
}

account system_program( std::string_view str ) noexcept
{
  protocol::account a{};
  a.at( 0 ) = native_program_account_prefix;

  std::size_t length = std::min( str.length(), a.size() - 1 );
  for( std::size_t i = 0; i < length; ++i )
    a.at( i + 1 ) = static_cast< std::byte >( str[ i ] );

  return a;
}

} // namespace koinos::protocol
