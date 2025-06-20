#pragma once

#include <span>

#include <koinos/crypto/hash.hpp>

namespace koinos::crypto {

constexpr std::size_t public_key_length = 32;
constexpr std::size_t signature_length  = 64;

using public_key_span = std::span< const std::byte, public_key_length >;
using signature       = std::array< std::byte, signature_length >;

class public_key
{
public:
  public_key() = delete;
  public_key( public_key_span pks ) noexcept;
  public_key( const public_key& pkv ) noexcept = default;
  public_key( public_key&& pkv ) noexcept      = default;
  ~public_key() noexcept                       = default;

  public_key& operator=( const public_key& pkv ) noexcept = default;
  public_key& operator=( public_key&& pkv ) noexcept      = default;

  bool operator==( const public_key& rhs ) const noexcept;
  bool operator!=( const public_key& rhs ) const noexcept;

  bool verify( const signature& sig, const digest& dig ) const noexcept;
  public_key_span bytes() const noexcept;

private:
  public_key_span _bytes;
};

} // namespace koinos::crypto
