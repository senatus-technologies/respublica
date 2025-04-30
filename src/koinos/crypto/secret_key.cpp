#include <koinos/crypto/secret_key.hpp>
#include <sodium.h>

namespace koinos::crypto {

using koinos::error::error_code;

static void initialize_crypto()
{
  [[maybe_unused]]
  static int retval = sodium_init();
  assert( retval >= 0 );
}

secret_key::secret_key( const secret_key_data& secret_bytes, const public_key_data& public_bytes ):
    _secret_bytes( secret_bytes ),
    _public_bytes( public_bytes )
{
  initialize_crypto();
}

secret_key::secret_key( secret_key_data&& secret_bytes, public_key_data&& public_bytes ):
    _secret_bytes( std::move( secret_bytes ) ),
    _public_bytes( std::move( public_bytes ) )
{
  initialize_crypto();
}

bool secret_key::operator==( const secret_key& rhs ) const
{
  return !std::memcmp( _secret_bytes.data(), rhs._secret_bytes.data(), secret_key_length )
         && !std::memcmp( _public_bytes.data(), rhs._public_bytes.data(), public_key_length );
}

bool secret_key::operator!=( const secret_key& rhs ) const
{
  return !( *this == rhs );
}

std::expected< secret_key, error > secret_key::create()
{
  public_key_data public_bytes;
  secret_key_data secret_bytes;

  if( crypto_sign_keypair( reinterpret_cast< unsigned char* >( public_bytes.data() ),
                           reinterpret_cast< unsigned char* >( secret_bytes.data() ) )
      < 0 )
    return std::unexpected( error_code::reversion );

  return secret_key( std::move( secret_bytes ), std::move( public_bytes ) );
}

std::expected< secret_key, error > secret_key::create( const multihash& seed )
{
  if( seed.digest().size() != crypto_sign_SEEDBYTES )
    return std::unexpected( error_code::reversion );

  public_key_data public_bytes;
  secret_key_data secret_bytes;

  if( crypto_sign_seed_keypair( reinterpret_cast< unsigned char* >( public_bytes.data() ),
                                reinterpret_cast< unsigned char* >( secret_bytes.data() ),
                                reinterpret_cast< const unsigned char* >( seed.digest().data() ) )
      < 0 )
    return std::unexpected( error_code::reversion );

  return secret_key( std::move( secret_bytes ), std::move( public_bytes ) );
}

secret_key_data secret_key::bytes() const
{
  return _secret_bytes;
}

public_key secret_key::public_key()
{
  return crypto::public_key( _public_bytes );
}

std::expected< signature, error > secret_key::sign( const multihash& mh ) const
{
  signature sig;
  if( crypto_sign_detached( reinterpret_cast< unsigned char* >( sig.data() ),
                            nullptr,
                            reinterpret_cast< const unsigned char* >( mh.digest().data() ),
                            mh.digest().size(),
                            reinterpret_cast< const unsigned char* >( _secret_bytes.data() ) )
      < 0 )
    return std::unexpected( error_code::reversion );

  return sig;
}

} // namespace koinos::crypto
