#include <koinos/crypto/secret_key.hpp>
#include <koinos/memory.hpp>

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
  static int retcode = sodium_init();
  assert( retcode >= 0 );
}
#endif

secret_key::secret_key( const secret_key_data& secret_bytes, const public_key_data& public_bytes ) noexcept:
    _public_bytes( public_bytes ),
    _secret_bytes( secret_bytes )
{
#ifndef FAST_CRYPTO
  initialize_crypto();
#endif
}

bool secret_key::operator==( const secret_key& rhs ) const noexcept
{
  return !std::memcmp( _secret_bytes.data(), rhs._secret_bytes.data(), secret_key_length )
         && !std::memcmp( _public_bytes.data(), rhs._public_bytes.data(), public_key_length );
}

bool secret_key::operator!=( const secret_key& rhs ) const noexcept
{
  return !( *this == rhs );
}

secret_key secret_key::create() noexcept
{
  secret_key new_key;
#ifdef FAST_CRYPTO
  [[maybe_unused]]
  ECCRYPTO_STATUS retcode =
    SchnorrQ_FullKeyGeneration( memory::pointer_cast< unsigned char* >( new_key._secret_bytes.data() ),
                                memory::pointer_cast< unsigned char* >( new_key._public_bytes.data() ) );

  assert( retcode == ECCRYPTO_SUCCESS );
#else
  [[maybe_unused]]
  int retcode = crypto_sign_keypair( memory::pointer_cast< unsigned char* >( new_key._public_bytes.data() ),
                                     memory::pointer_cast< unsigned char* >( new_key._secret_bytes.data() ) );
  assert( retcode >= 0 );
#endif

  return new_key;
}

secret_key secret_key::create( const digest& seed ) noexcept
{
#ifdef FAST_CRYPTO
  secret_key new_key( seed, public_key_data{} );
  [[maybe_unused]]
  ECCRYPTO_STATUS retcode =
    SchnorrQ_KeyGeneration( memory::pointer_cast< unsigned char* >( new_key._secret_bytes.data() ),
                            memory::pointer_cast< unsigned char* >( new_key._public_bytes.data() ) );
  assert( retcode == ECCRYPTO_SUCCESS );
#else
  secret_key new_key;

  [[maybe_unused]]
  int retcode = crypto_sign_seed_keypair( memory::pointer_cast< unsigned char* >( new_key._public_bytes.data() ),
                                          memory::pointer_cast< unsigned char* >( new_key._secret_bytes.data() ),
                                          memory::pointer_cast< const unsigned char* >( seed.data() ) );
  assert( retcode >= 0 );
#endif

  return new_key;
}

secret_key_data secret_key::bytes() const noexcept
{
  return _secret_bytes;
}

public_key secret_key::public_key() const noexcept
{
  return crypto::public_key( public_key_span( _public_bytes ) );
}

signature secret_key::sign( const digest& d ) const noexcept
{
  signature sig;

#ifdef FAST_CRYPTO
  [[maybe_unused]]
  ECCRYPTO_STATUS retcode = SchnorrQ_Sign( memory::pointer_cast< const unsigned char* >( _secret_bytes.data() ),
                                           memory::pointer_cast< const unsigned char* >( _public_bytes.data() ),
                                           memory::pointer_cast< const unsigned char* >( d.data() ),
                                           d.size(),
                                           memory::pointer_cast< unsigned char* >( sig.data() ) );
  assert( retcode == ECCRYPTO_SUCCESS );
#else
  [[maybe_unused]]
  auto retcode = crypto_sign_detached( memory::pointer_cast< unsigned char* >( sig.data() ),
                                       nullptr,
                                       memory::pointer_cast< const unsigned char* >( d.data() ),
                                       d.size(),
                                       memory::pointer_cast< const unsigned char* >( _secret_bytes.data() ) );
  assert( retcode >= 0 );
#endif

  return sig;
}

} // namespace koinos::crypto
