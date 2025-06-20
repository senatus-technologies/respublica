#pragma once

#include "koinos/crypto/public_key.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto.hpp>

namespace koinos::protocol {

struct account;
struct user_account;
struct program_account;

struct account: public std::array< std::byte, crypto::public_key_length + 1 >
{
  explicit operator crypto::public_key() const noexcept;

  bool user() const noexcept;
  bool program() const noexcept;
};

struct user_account: public account
{
  user_account( const crypto::public_key& ) noexcept;
  user_account( const account& ) noexcept;
  ~user_account() = default;

  user_account()                      = delete;
  user_account( const user_account& ) = delete;
  user_account( user_account&& )      = delete;

  user_account& operator=( const user_account& ) = delete;
  user_account& operator=( user_account&& )      = delete;
};

struct program_account: public account
{
  program_account( const crypto::public_key& ) noexcept;
  program_account( const account& ) noexcept;
  ~program_account() = default;

  program_account()                         = delete;
  program_account( const program_account& ) = delete;
  program_account( program_account&& )      = delete;

  program_account& operator=( const program_account& ) = delete;
  program_account& operator=( program_account&& )      = delete;
};

struct account_view: public std::span< const std::byte, crypto::public_key_length + 1 >
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
