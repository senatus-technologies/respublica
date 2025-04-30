#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>

#include <sodium.h>

namespace koinos::crypto {

using koinos::error::error_code;

static void initialize_crypto()
{
  [[maybe_unused]]
  static int retval = sodium_init();
  assert( retval >= 0 );
}

public_key::public_key()
{
  initialize_crypto();
}

public_key::public_key( const public_key& pk ):
    _bytes( pk._bytes )
{
  initialize_crypto();
}

public_key::public_key( public_key&& pk ):
    _bytes( std::move( pk._bytes ) )
{
  initialize_crypto();
}

public_key::public_key( const public_key_data& pkd ):
    _bytes( pkd )
{
  initialize_crypto();
}

public_key::public_key( public_key_data&& pkd ):
    _bytes( std::move( pkd ) )
{
  initialize_crypto();
}

public_key& public_key::operator=( public_key&& pk )
{
  _bytes = std::move( pk._bytes );
  return *this;
}

public_key& public_key::operator=( const public_key& pk )
{
  _bytes = pk._bytes;
  return *this;
}

bool public_key::operator==( const public_key& rhs ) const
{
  return std::memcmp( _bytes.data(), rhs._bytes.data(), public_key_length ) == 0;
}

bool public_key::operator!=( const public_key& rhs ) const
{
  return !( *this == rhs );
}

public_key::~public_key() {}

bool public_key::valid() const
{
  static const public_key_data empty_pub{};
  return _bytes != empty_pub;
}

public_key_data public_key::bytes() const
{
  return _bytes;
}

bool public_key::verify( const signature& sig, const multihash& mh ) const
{
  return !crypto_sign_verify_detached( reinterpret_cast< const unsigned char* >( sig.data() ),
                                       reinterpret_cast< const unsigned char* >( mh.digest().data() ),
                                       mh.digest().size(),
                                       reinterpret_cast< const unsigned char* >( _bytes.data() ) );
}

private_key::private_key()
{
  initialize_crypto();
}

private_key::private_key( private_key&& pk ):
    _secret_bytes( std::move( pk._secret_bytes ) ),
    _public_bytes( std::move( pk._public_bytes ) )
{
  initialize_crypto();
}

private_key::private_key( const private_key& pk ):
    _secret_bytes( pk._secret_bytes ),
    _public_bytes( pk._public_bytes )
{
  initialize_crypto();
}

private_key::~private_key() {}

private_key& private_key::operator=( private_key&& pk )
{
  _secret_bytes = std::move( pk._secret_bytes );
  _public_bytes = std::move( pk._public_bytes );
  return *this;
}

private_key& private_key::operator=( const private_key& pk )
{
  _secret_bytes = pk._secret_bytes;
  _public_bytes = pk._public_bytes;
  return *this;
}

bool private_key::operator==( const private_key& rhs ) const
{
  return !std::memcmp( _secret_bytes.data(), rhs._secret_bytes.data(), secret_key_length )
         && !std::memcmp( _public_bytes.data(), rhs._public_bytes.data(), public_key_length );
}

bool private_key::operator!=( const private_key& rhs ) const
{
  return !( *this == rhs );
}

std::expected< private_key, error > private_key::create()
{
  private_key self;

  if( crypto_sign_keypair( reinterpret_cast< unsigned char* >( self._public_bytes.data() ),
                           reinterpret_cast< unsigned char* >( self._secret_bytes.data() ) )
      < 0 )
    return std::unexpected( error_code::reversion );

  return self;
}

std::expected< private_key, error > private_key::create( const multihash& seed )
{
  if( seed.digest().size() != crypto_sign_SEEDBYTES )
    return std::unexpected( error_code::reversion );

  private_key self;

  if( crypto_sign_seed_keypair( reinterpret_cast< unsigned char* >( self._public_bytes.data() ),
                                reinterpret_cast< unsigned char* >( self._secret_bytes.data() ),
                                reinterpret_cast< const unsigned char* >( seed.digest().data() ) )
      < 0 )
    return std::unexpected( error_code::reversion );

  return self;
}

secret_key_data private_key::bytes() const
{
  return _secret_bytes;
}

public_key private_key::public_key()
{
  return crypto::public_key( _public_bytes );
}

std::expected< signature, error > private_key::sign( const multihash& mh ) const
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

namespace koinos {

template<>
void to_binary< crypto::public_key >( std::ostream& s, const crypto::public_key& k )
{
  crypto::public_key_data bytes = k.bytes();
  s.write( reinterpret_cast< const char* >( bytes.data() ), bytes.size() );
}

template<>
void from_binary< crypto::public_key >( std::istream& s, crypto::public_key& k )
{
  crypto::public_key_data bytes;
  s.read( reinterpret_cast< char* >( bytes.data() ), bytes.size() );
  k = std::move( crypto::public_key( std::move( bytes ) ) );
}

} // namespace koinos
