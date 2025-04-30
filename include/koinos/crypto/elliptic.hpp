#pragma once

#include <expected>

#include <koinos/binary.hpp>
#include <koinos/crypto/multihash.hpp>

namespace koinos::crypto {

constexpr std::size_t public_key_length = 32;
constexpr std::size_t secret_key_length = 64;
constexpr std::size_t signature_length  = 64;

using secret_key_data = std::array< std::byte, secret_key_length >;
using public_key_data = std::array< std::byte, public_key_length >;
using signature       = std::array< std::byte, signature_length >;

class public_key
{
public:
  public_key()                      = delete;
  public_key( public_key&& pk )     = default;
  public_key( const public_key& k ) = default;
  public_key( public_key_data&& pkd );
  public_key( const public_key_data& pkd );
  ~public_key() = default;

  public_key& operator=( public_key&& pk )      = default;
  public_key& operator=( const public_key& pk ) = default;

  bool operator==( const public_key& rhs ) const;
  bool operator!=( const public_key& rhs ) const;

  bool verify( const signature& sig, const multihash& mh ) const;
  public_key_data bytes() const;

private:
  public_key_data _bytes;
};

class private_key
{
public:
  private_key()                        = delete;
  private_key( private_key&& pk )      = default;
  private_key( const private_key& pk ) = default;
  private_key( secret_key_data&& secret_bytes, public_key_data&& public_bytes );
  private_key( const secret_key_data& secret_bytes, const public_key_data& public_bytes );
  ~private_key() = default;

  private_key& operator=( private_key&& pk )      = default;
  private_key& operator=( const private_key& pk ) = default;

  bool operator==( const private_key& rhs ) const;
  bool operator!=( const private_key& rhs ) const;

  static std::expected< private_key, error > create();
  static std::expected< private_key, error > create( const multihash& seed );

  std::expected< signature, error > sign( const multihash& digest ) const;
  public_key public_key();
  secret_key_data bytes() const;

private:
  public_key_data _public_bytes;
  secret_key_data _secret_bytes;
};

} // namespace koinos::crypto

namespace koinos {

template<>
void to_binary< crypto::public_key >( std::ostream& s, const crypto::public_key& k );

template<>
void from_binary< crypto::public_key >( std::istream& s, crypto::public_key& k );

} // namespace koinos
