#include "koinos/crypto/public_key.hpp"
#include <koinos/protocol/account.hpp>

namespace koinos::protocol {

// Using constexpr because std::byte cannot be a enumeration base type
constexpr auto user_account_prefix    = std::byte{ 0x00 };
constexpr auto program_account_prefix = std::byte{ 0x01 };

account::operator crypto::public_key() const noexcept
{
  return crypto::public_key( crypto::public_key_span( begin() + 1, end() ) );
}

bool account::user() const noexcept
{
  return at( 0 ) == user_account_prefix;
}

bool account::program() const noexcept
{
  return at( 0 ) == program_account_prefix;
}

user_account::user_account( const crypto::public_key& pub_key ) noexcept:
    account{ user_account_prefix }
{
  std::ranges::copy( pub_key.bytes(), begin() + 1 );
}

user_account::user_account( const account& acc ) noexcept:
    account( acc )
{
  at( 0 ) = user_account_prefix;
}

program_account::program_account( const crypto::public_key& pub_key ) noexcept:
    account{ program_account_prefix }
{
  std::ranges::copy( pub_key.bytes(), begin() + 1 );
}

program_account::program_account( const account& acc ) noexcept:
    account( acc )
{
  at( 0 ) = program_account_prefix;
}

account_view::account_view( const account& acc ) noexcept:
    std::span< const std::byte, crypto::public_key_length + 1 >( acc )
{}

account_view::account_view( const std::byte* ptr, std::size_t length ) noexcept:
    std::span< const std::byte, crypto::public_key_length + 1 >( ptr, length )
{}

account_view::operator crypto::public_key() const noexcept
{
  return crypto::public_key( crypto::public_key_span( begin() + 1, end() ) );
}

bool account_view::user() const noexcept
{
  return ( *this )[ 0 ] == user_account_prefix;
}

bool account_view::program() const noexcept
{
  return ( *this )[ 0 ] == program_account_prefix;
}

account system_program( std::string_view str ) noexcept
{
  protocol::account a{ program_account_prefix };
  std::size_t length = std::min( str.length(), a.size() - 1 );
  for( std::size_t i = 0; i < length; ++i )
    a.at( i + 1 ) = static_cast< std::byte >( str[ i ] );

  return a;
}

} // namespace koinos::protocol
