#pragma once

#include <span>

#include <koinos/crypto/hash.hpp>

namespace koinos::crypto {

constexpr std::size_t public_key_length = 32;
constexpr std::size_t signature_length  = 64;

using public_key_span = std::span< const std::byte, public_key_length >;
using signature       = std::array< std::byte, signature_length >;

class public_key_view
{
public:
  public_key_view() = delete;
  public_key_view( public_key_span pks ) noexcept;
  public_key_view( const public_key_view& pkv ) noexcept = default;
  public_key_view( public_key_view&& pkv ) noexcept = default;
  ~public_key_view() noexcept = default;

  public_key_view& operator=( const public_key_view& pkv ) noexcept = default;
  public_key_view& operator=( public_key_view&& pkv ) noexcept      = default;

  bool operator==( const public_key_view& rhs ) const noexcept;
  bool operator!=( const public_key_view& rhs ) const noexcept;

  bool verify( const signature& sig, const digest& dig ) const noexcept;
  public_key_span bytes() const noexcept;

private:
  public_key_span _bytes;
};

}