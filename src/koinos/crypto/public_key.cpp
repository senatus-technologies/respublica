#include <koinos/crypto/public_key.hpp>
#include <koinos/memory/memory.hpp>

#include <cassert>
#include <cstring>

#ifdef FAST_CRYPTO
#  include <fourq/FourQ_api.h>
#else
#  include <sodium.h>
#endif

namespace koinos::crypto {

#ifndef FAST_CRYPTO
static void initialize_crypto()
{
  [[maybe_unused]]
  static int retval = sodium_init();
  assert( retval >= 0 );
}
#endif

public_key::public_key( const public_key_data& pkd ) noexcept:
    _bytes( pkd )
{
#ifndef FAST_CRYPTO
  initialize_crypto();
#endif
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
#ifdef FAST_CRYPTO
  unsigned int valid = 0;
  [[maybe_unused]]
  ECCRYPTO_STATUS retcode = SchnorrQ_Verify( memory::pointer_cast< const unsigned char* >( _bytes.data() ),
                                             memory::pointer_cast< const unsigned char* >( d.data() ),
                                             d.size(),
                                             memory::pointer_cast< const unsigned char* >( sig.data() ),
                                             &valid );
  assert( retcode = ECCRYPTO_SUCCESS );
  return valid == 1;
#else
  return !crypto_sign_verify_detached( memory::pointer_cast< const unsigned char* >( sig.data() ),
                                       memory::pointer_cast< const unsigned char* >( d.data() ),
                                       d.size(),
                                       memory::pointer_cast< const unsigned char* >( _bytes.data() ) );
#endif
}

} // namespace koinos::crypto
