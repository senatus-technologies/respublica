#include <koinos/crypto/secret_key.hpp>
#include <koinos/util/memory.hpp>

#include <cassert>
#include <cstring>

#include <sodium.h>

namespace koinos::crypto {

static void initialize_crypto()
{
  [[maybe_unused]]
  static int retcode = sodium_init();
  assert( retcode >= 0 );
}

secret_key::secret_key( const secret_key_data& secret_bytes, const public_key_data& public_bytes ) noexcept:
    _public_bytes( public_bytes ),
    _secret_bytes( secret_bytes )
{
  initialize_crypto();
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

  [[maybe_unused]]
  int retcode = crypto_sign_keypair( util::pointer_cast< unsigned char* >( new_key._public_bytes.data() ),
                                     util::pointer_cast< unsigned char* >( new_key._secret_bytes.data() ) );
  assert( retcode >= 0 );

  return new_key;
}

secret_key secret_key::create( const digest& seed ) noexcept
{
  assert( seed.size() == crypto_sign_SEEDBYTES );

  secret_key new_key;

  [[maybe_unused]]
  int retcode = crypto_sign_seed_keypair( util::pointer_cast< unsigned char* >( new_key._public_bytes.data() ),
                                          util::pointer_cast< unsigned char* >( new_key._secret_bytes.data() ),
                                          util::pointer_cast< const unsigned char* >( seed.data() ) );
  assert( retcode >= 0 );

  return new_key;
}

secret_key_data secret_key::bytes() const noexcept
{
  return _secret_bytes;
}

public_key secret_key::public_key() const noexcept
{
  return crypto::public_key( _public_bytes );
}

signature secret_key::sign( const digest& d ) const noexcept
{
  signature sig;

  [[maybe_unused]]
  auto retcode = crypto_sign_detached( util::pointer_cast< unsigned char* >( sig.data() ),
                                       nullptr,
                                       util::pointer_cast< const unsigned char* >( d.data() ),
                                       d.size(),
                                       util::pointer_cast< const unsigned char* >( _secret_bytes.data() ) );
  assert( retcode >= 0 );

  return sig;
}

} // namespace koinos::crypto
