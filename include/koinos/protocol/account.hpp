#pragma once

#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto.hpp>

namespace koinos::protocol {

struct account: std::array< std::byte, crypto::public_key_length + 1 >
{
  explicit operator crypto::public_key() const noexcept;

  bool user() const noexcept;
  bool program() const noexcept;
};

account user_account( const crypto::public_key& ) noexcept;
account user_account( const account& ) noexcept;

account program_account( const crypto::public_key& ) noexcept;
account program_account( const account& ) noexcept;

struct account_view: std::span< const std::byte, crypto::public_key_length + 1 >
{
  account_view( const account& ) noexcept;
  account_view( const std::byte*, std::size_t ) noexcept;

  explicit operator crypto::public_key() const noexcept;

  bool user() const noexcept;
  bool program() const noexcept;
};

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

account system_program( std::string_view str ) noexcept;

} // namespace koinos::protocol
