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
  public_key();
  public_key( public_key&& pk );
  public_key( const public_key& k );
  public_key( public_key_data&& pkd );
  public_key( const public_key_data& pkd );
  ~public_key();

  public_key& operator=( public_key&& pk );
  public_key& operator=( const public_key& pk );

  bool operator==( const public_key& rhs ) const;
  bool operator!=( const public_key& rhs ) const;

  bool verify( const signature& sig, const multihash& mh ) const;

  bool valid() const;

  public_key_data bytes() const;

private:
  public_key_data _bytes;
};

class private_key
{
public:
  private_key();
  private_key( private_key&& pk );
  private_key( const private_key& pk );
  private_key( secret_key_data&& pkd );
  private_key( const secret_key_data& pkd );
  ~private_key();

  private_key& operator=( private_key&& pk );
  private_key& operator=( const private_key& pk );

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
