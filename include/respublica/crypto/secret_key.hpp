#pragma once

#include <array>

#include <respublica/crypto/hash.hpp>
#include <respublica/crypto/public_key.hpp>

namespace respublica::crypto {

#ifdef FAST_CRYPTO
constexpr std::size_t secret_key_length = 32;
#else
constexpr std::size_t secret_key_length = 64;
#endif

using public_key_data = std::array< std::byte, public_key_length >;
using secret_key_data = std::array< std::byte, secret_key_length >;

class secret_key
{
public:
  secret_key()                                = default;
  secret_key( secret_key&& pk ) noexcept      = default;
  secret_key( const secret_key& pk ) noexcept = default;
  secret_key( const secret_key_data& secret_bytes, const public_key_data& public_bytes ) noexcept;
  ~secret_key() noexcept = default;

  secret_key& operator=( secret_key&& pk ) noexcept      = default;
  secret_key& operator=( const secret_key& pk ) noexcept = default;

  bool operator==( const secret_key& rhs ) const noexcept;
  bool operator!=( const secret_key& rhs ) const noexcept;

  static secret_key create() noexcept;
  static secret_key create( const digest& seed ) noexcept;

  signature sign( const digest& digest ) const noexcept;
  crypto::public_key public_key() const noexcept;
  secret_key_data bytes() const noexcept;

private:
  public_key_data _public_bytes{};
  secret_key_data _secret_bytes{};
};

} // namespace respublica::crypto
