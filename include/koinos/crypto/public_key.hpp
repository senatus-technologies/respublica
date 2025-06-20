#pragma once

#include <array>
#include <expected>

#include <koinos/crypto/hash.hpp>
#include <koinos/crypto/public_key_view.hpp>

namespace koinos::crypto {

using public_key_data = std::array< std::byte, public_key_length >;

class public_key
{
public:
  public_key() noexcept                      = default;
  public_key( public_key&& pk ) noexcept     = default;
  public_key( const public_key& pk ) noexcept = default;
  public_key( const public_key_data& pkd ) noexcept;
  ~public_key() noexcept = default;

  public_key& operator=( public_key&& pk ) noexcept      = default;
  public_key& operator=( const public_key& pk ) noexcept = default;

  bool operator==( const public_key& rhs ) const noexcept;
  bool operator!=( const public_key& rhs ) const noexcept;

  bool verify( const signature& sig, const digest& dig ) const noexcept;
  const public_key_data& bytes() const noexcept;

private:
  public_key_data _bytes{};
};

bool operator==( const public_key& lhs, const public_key_view& rhs ) noexcept;
bool operator!=( const public_key& lhs, const public_key_view& rhs ) noexcept;
bool operator==( const public_key_view& lhs, const public_key& rhs ) noexcept;
bool operator!=( const public_key_view& lhs, const public_key& rhs ) noexcept;

} // namespace koinos::crypto
