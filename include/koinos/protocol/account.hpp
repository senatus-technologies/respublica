#pragma once

#include <cstdint>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto.hpp>

namespace koinos::protocol {

using account = std::array< std::byte, crypto::public_key_length + 1 >;

// Using constexpr because std::byte cannot be a enumeration base type
constexpr auto user_account_prefix    = std::byte{ 0x00 };
constexpr auto program_account_prefix = std::byte{ 0x01 };

struct authorization
{
  account signer{};
  crypto::signature signature{};

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & signer;
    ar & signature;
  }

  constexpr std::size_t size() const noexcept
  {
    std::size_t bytes = 0;

    bytes += signer.size();
    bytes += signature.size();

    return bytes;
  }
};

inline account user_account( crypto::public_key pub_key ) noexcept
{
  account a{ user_account_prefix };
  std::ranges::copy( pub_key.bytes(), a.begin() + 1 );
  return a;
}

inline account program_account( crypto::public_key pub_key ) noexcept
{
  account a{ program_account_prefix };
  std::ranges::copy( pub_key.bytes(), a.begin() + 1 );
  return a;
}

inline bool is_user( const account& acc ) noexcept
{
  return acc.at( 0 ) == user_account_prefix;
}

inline bool is_program( const account& acc ) noexcept
{
  return acc.at( 0 ) == program_account_prefix;
}

inline crypto::public_key as_public_key( const account& acc ) noexcept
{
  return crypto::public_key( crypto::public_key_span( acc.begin() + 1, acc.end() ) );
}

constexpr inline protocol::account system_program( std::string_view str ) noexcept
{
  protocol::account a{ program_account_prefix };
  std::size_t length = std::min( str.length(), a.size() - 1 );
  for( std::size_t i = 0; i < length; ++i )
    a.at( i + 1 ) = static_cast< std::byte >( str[ i ] );

  return a;
}

} // namespace koinos::protocol
