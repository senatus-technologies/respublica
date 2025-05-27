#include <koinos/crypto/public_key.hpp>
#include <koinos/util/conversion.hpp>

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

bool public_key::verify( const signature& sig, const multihash& mh ) const noexcept
{
  return !crypto_sign_verify_detached( reinterpret_cast< const unsigned char* >( sig.data() ),
                                       reinterpret_cast< const unsigned char* >( mh.digest().data() ),
                                       mh.digest().size(),
                                       reinterpret_cast< const unsigned char* >( _bytes.data() ) );
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
