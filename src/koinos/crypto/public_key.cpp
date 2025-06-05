#include <koinos/crypto/public_key.hpp>

#include <sodium.h>

namespace koinos::crypto {

static void initialize_crypto()
{
  [[maybe_unused]]
  static int retval = sodium_init();
  assert( retval >= 0 );
}

public_key::public_key( const public_key_data& pkd ) noexcept:
    _bytes( pkd )
{
  initialize_crypto();
}

public_key::public_key( public_key_data&& pkd ) noexcept:
    _bytes( std::move( pkd ) )
{
  initialize_crypto();
}

bool public_key::operator==( const public_key& rhs ) const noexcept
{
  return std::memcmp( _bytes.data(), rhs._bytes.data(), public_key_length ) == 0;
}

bool public_key::operator!=( const public_key& rhs ) const noexcept
{
  return !( *this == rhs );
}

const public_key_data& public_key::bytes() const noexcept
{
  return _bytes;
}

bool public_key::verify( const signature& sig, const digest& d ) const noexcept
{
  return !crypto_sign_verify_detached( reinterpret_cast< const unsigned char* >( sig.data() ),
                                       reinterpret_cast< const unsigned char* >( d.data() ),
                                       d.size(),
                                       reinterpret_cast< const unsigned char* >( _bytes.data() ) );
}

} // namespace koinos::crypto
