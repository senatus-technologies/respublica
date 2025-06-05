#pragma once

#include <array>
#include <expected>

#include <koinos/crypto/hash.hpp>

namespace koinos::crypto {

constexpr std::size_t public_key_length = 32;
constexpr std::size_t signature_length  = 64;

using public_key_data = std::array< std::byte, public_key_length >;
using signature       = std::array< std::byte, signature_length >;

class public_key
{
public:
  public_key() noexcept                      = default;
  public_key( public_key&& pk ) noexcept     = default;
  public_key( const public_key& k ) noexcept = default;
  public_key( public_key_data&& pkd ) noexcept;
  public_key( const public_key_data& pkd ) noexcept;
  ~public_key() noexcept = default;

  public_key& operator=( public_key&& pk ) noexcept      = default;
  public_key& operator=( const public_key& pk ) noexcept = default;

  bool operator==( const public_key& rhs ) const noexcept;
  bool operator!=( const public_key& rhs ) const noexcept;

  bool verify( const signature& sig, const digest& mh ) const noexcept;
  const public_key_data& bytes() const noexcept;

private:
  public_key_data _bytes;
};

} // namespace koinos::crypto
