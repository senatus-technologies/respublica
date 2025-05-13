#pragma once
#include <array>
#include <expected>

#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/public_key.hpp>
#include <koinos/error/error.hpp>

namespace koinos::crypto {

constexpr std::size_t secret_key_length = 64;
using secret_key_data                   = std::array< std::byte, secret_key_length >;

class secret_key
{
public:
  secret_key()                                = delete;
  secret_key( secret_key&& pk ) noexcept      = default;
  secret_key( const secret_key& pk ) noexcept = default;
  secret_key( secret_key_data&& secret_bytes, public_key_data&& public_bytes ) noexcept;
  secret_key( const secret_key_data& secret_bytes, const public_key_data& public_bytes ) noexcept;
  ~secret_key() noexcept = default;

  secret_key& operator=( secret_key&& pk ) noexcept      = default;
  secret_key& operator=( const secret_key& pk ) noexcept = default;

  bool operator==( const secret_key& rhs ) const noexcept;
  bool operator!=( const secret_key& rhs ) const noexcept;

  static std::expected< secret_key, error > create() noexcept;
  static std::expected< secret_key, error > create( const multihash& seed ) noexcept;

  std::expected< signature, error > sign( const multihash& digest ) const noexcept;
  crypto::public_key public_key() const noexcept;
  secret_key_data bytes() const noexcept;

private:
  public_key_data _public_bytes;
  secret_key_data _secret_bytes;
};

} // namespace koinos::crypto
