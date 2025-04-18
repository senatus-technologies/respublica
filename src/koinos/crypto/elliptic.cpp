#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/openssl.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <secp256k1-vrf.h>
#include <secp256k1_recovery.h>

namespace koinos {
namespace crypto {

using namespace boost::multiprecision::literals;
using koinos::error::error_code;

const secp256k1_context* _get_context()
{
  static secp256k1_context* ctx = secp256k1_context_create( SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN );
  return ctx;
}

void _init_lib()
{
  static const secp256k1_context* ctx = _get_context();
  static int init_o                   = init_openssl();
  boost::ignore_unused( ctx, init_o );
}

static int extended_nonce_function( unsigned char* nonce32,
                                    const unsigned char* msg32,
                                    const unsigned char* key32,
                                    const unsigned char* algo16,
                                    void* data,
                                    const unsigned int attempt )
{
  unsigned int* extra = (unsigned int*)data;
  ( *extra )++;
  return secp256k1_nonce_function_default( nonce32, msg32, key32, algo16, nullptr, *extra );
}

namespace detail {

using public_key_data =
  std::array< std::byte, sizeof( secp256k1_pubkey ) >; ///< The full non-compressed ECDSA public key point

const public_key_data& empty_pub()
{
  static const public_key_data empty_pub{};
  return empty_pub;
}

struct public_key_impl
{
  public_key_impl();
  public_key_impl( const public_key_impl& k );
  public_key_impl( public_key_impl&& pk );

  ~public_key_impl();

  std::expected< compressed_public_key, error > serialize() const;
  std::expected< uncompressed_public_key, error > serialize_uncompressed() const;

  std::expected< public_key_impl, error > add( const multihash& offset ) const;

  bool valid() const;

  friend bool operator==( const public_key_impl& a, const public_key_impl& b )
  {
    return std::memcmp( a._key.data(), b._key.data(), sizeof( public_key_data ) ) == 0;
  }

  std::vector< std::byte > to_address_bytes( std::byte prefix ) const;

  unsigned int fingerprint() const;

  public_key_data _key;
};

public_key_impl::public_key_impl()
{
  _init_lib();
}

public_key_impl::public_key_impl( const public_key_impl& pk ):
    _key( pk._key )
{
  _init_lib();
}

public_key_impl::public_key_impl( public_key_impl&& pk ):
    _key( std::move( pk._key ) )
{
  _init_lib();
}

public_key_impl::~public_key_impl() {}

std::expected< compressed_public_key, error > public_key_impl::serialize() const
{
  if( _key == empty_pub() )
    return std::unexpected( error_code::reversion ); // "cannot serialize an empty public key"

  compressed_public_key cpk;
  size_t len = cpk.size();
  if( !secp256k1_ec_pubkey_serialize( _get_context(),
                                      reinterpret_cast< unsigned char* >( cpk.data() ),
                                      &len,
                                      reinterpret_cast< const secp256k1_pubkey* >( _key.data() ),
                                      SECP256K1_EC_COMPRESSED ) )
    return std::unexpected( error_code::reversion ); // "unknown error during public key serialization"

  if( len != cpk.size() )
    return std::unexpected( error_code::reversion ); // "serialized key does not match expected size of ${n} bytes"

  return cpk;
}

std::expected< uncompressed_public_key, error > public_key_impl::serialize_uncompressed() const
{
  if( _key == empty_pub() )
    return std::unexpected( error_code::reversion ); // "cannot serialize an empty public key"

  uncompressed_public_key upk;
  size_t len = upk.size();
  if( !secp256k1_ec_pubkey_serialize( _get_context(),
                                      reinterpret_cast< unsigned char* >( upk.data() ),
                                      &len,
                                      reinterpret_cast< const secp256k1_pubkey* >( _key.data() ),
                                      SECP256K1_EC_UNCOMPRESSED ) )
    return std::unexpected( error_code::reversion ); // "unknown error during public key serialization"

  if( len != upk.size() )
    return std::unexpected( error_code::reversion ); // "serialized key does not match expected size of ${n} bytes"

  return upk;
}

std::expected< public_key_impl, error > public_key_impl::add( const multihash& hash ) const
{
  if( hash.digest().size() != 32 )
    return std::unexpected( error_code::reversion ); // "digest must be 32 bytes"
  if( _key == empty_pub() )
    return std::unexpected( error_code::reversion ); // "cannot add to an empty key"

  public_key_impl new_key( *this );
  if( !secp256k1_ec_pubkey_tweak_add( _get_context(),
                                     (secp256k1_pubkey*)new_key._key.data(),
                                     (unsigned char*)hash.digest().data() ) )
    return std::unexpected( error_code::reversion ); // "unknown error when adding to public key"

  return new_key;
}

bool public_key_impl::valid() const
{
  return _key != empty_pub();
}

std::vector< std::byte > public_key_impl::to_address_bytes( std::byte prefix ) const
{
  auto compressed_key = serialize();
  if( !compressed_key )
    throw std::runtime_error( std::string( compressed_key.error().message() ) );

  auto sha256 = hash( multicodec::sha2_256, (char*)compressed_key->data(), compressed_key->size() );
  if( !sha256 )
    throw std::runtime_error( std::string( sha256.error().message() ) );

  auto ripemd160 = hash( multicodec::ripemd_160, *sha256 );
  if( !ripemd160 )
    throw std::runtime_error( std::string( ripemd160.error().message() ) );

  std::vector< std::byte > d;
  d.resize( 25 );
  d[ 0 ] = prefix;
  std::memcpy( d.data() + 1, ripemd160->digest().data(), ripemd160->digest().size() );
  sha256 = hash( multicodec::sha2_256, (char*)d.data(), ripemd160->digest().size() + 1 );
  if( !sha256 )
    throw std::runtime_error( std::string( sha256.error().message() ) );

  sha256 = hash( multicodec::sha2_256, *sha256 );
  if( !sha256 )
    throw std::runtime_error( std::string( sha256.error().message() ) );

  std::memcpy( d.data() + ripemd160->digest().size() + 1, sha256->digest().data(), 4 );
  return d;
}

unsigned int public_key_impl::fingerprint() const
{
  auto sha256 = hash( multicodec::sha2_256, (char*)_key.data(), _key.size() );
  if( !sha256 )
    throw std::runtime_error( std::string( sha256.error().message() ) );

  auto ripemd160 = hash( multicodec::ripemd_160, *sha256 );
  if( !ripemd160 )
    throw std::runtime_error( std::string( ripemd160.error().message() ) );

  unsigned char* fp   = (unsigned char*)ripemd160->digest().data();
  return ( fp[ 0 ] << 24 ) | ( fp[ 1 ] << 16 ) | ( fp[ 2 ] << 8 ) | fp[ 3 ];
}
} // namespace detail

const private_key_secret& empty_priv()
{
  static const private_key_secret empty_priv{};
  return empty_priv;
}

public_key::public_key():
    _my( std::make_unique< detail::public_key_impl >() )
{}

public_key::public_key( const public_key& k ):
    _my( std::make_unique< detail::public_key_impl >( *k._my ) )
{}

public_key::public_key( public_key&& k ):
    _my( std::move( k._my ) )
{}

public_key::~public_key() {}

std::expected< compressed_public_key, error > public_key::serialize() const
{
  return _my->serialize();
}

std::expected< uncompressed_public_key, error > public_key::serialize_uncompressed() const
{
  return _my->serialize_uncompressed();
}

std::expected< public_key, error > public_key::deserialize( const compressed_public_key& cpk )
{
  public_key pk;
  if( !secp256k1_ec_pubkey_parse( _get_context(),
                                 reinterpret_cast< secp256k1_pubkey* >( pk._my->_key.data() ),
                                 reinterpret_cast< const unsigned char* >( cpk.data() ),
                                 cpk.size() ) )
    return std::unexpected( error_code::reversion ); // "unknown error during public key deserialization"
  return pk;
}

std::expected< public_key, error > public_key::deserialize( const uncompressed_public_key& upk )
{
  public_key pk;
  if( !secp256k1_ec_pubkey_parse( _get_context(),
                                  reinterpret_cast< secp256k1_pubkey* >( pk._my->_key.data() ),
                                  reinterpret_cast< const unsigned char* >( upk.data() ),
                                  upk.size() ) )
    return std::unexpected( error_code::reversion ); // "unknown error during public key deserialization"
  return pk;
}

std::expected< public_key, error > public_key::recover( const recoverable_signature& sig, const multihash& hash )
{
  if( hash.digest().size() != 32 )
    return std::unexpected( error_code::reversion ); // "digest must be 32 bytes"
  if( !is_canonical( sig ) )
    return std::unexpected( error_code::reversion ); // "signature is not canonical"

  std::array< std::byte, 65 > internal_sig;
  public_key pk;

  int32_t rec_id = std::to_integer< int32_t >( sig[ 0 ] );
  if( rec_id < 31 || 33 < rec_id )
    return std::unexpected( error_code::reversion ); // "recovery id mismatch, must be in range [31,33]"

  // The internal representation, as per the secp256k1 documentation, is an implementation
  // detail and not guaranteed across platforms or versions of secp256k1. We need to
  // convert the portable format to the internal format first.
  if(
    !secp256k1_ecdsa_recoverable_signature_parse_compact( _get_context(),
                                                          (secp256k1_ecdsa_recoverable_signature*)internal_sig.data(),
                                                          (const unsigned char*)sig.data() + 1,
                                                          rec_id >> 5 ) )
    return std::unexpected( error_code::reversion ); // "unknown error when parsing signature"

  if( !secp256k1_ecdsa_recover( _get_context(),
                                (secp256k1_pubkey*)pk._my->_key.data(),
                                (const secp256k1_ecdsa_recoverable_signature*)internal_sig.data(),
                                (unsigned char*)hash.digest().data() ) )
    return std::unexpected( error_code::reversion ); // "unknown error recovering public key from signature";

  return pk;
}

std::expected< public_key, error > public_key::add( const multihash& offset ) const
{
  return _my->add( offset ).and_then( []( auto&& key_impl ) -> std::expected< public_key, error >
    {
      public_key pk;
      pk._my = std::make_unique< detail::public_key_impl >( std::move( key_impl ) );
      return pk;
    });
}

bool public_key::valid() const
{
  return _my->valid();
}

std::expected< multihash, error > public_key::verify_random_proof( const std::string& input, const std::string& proof ) const
{
  if( proof.size() != 81 )
    return std::unexpected( error_code::reversion ); // "proof must be 81 bytes"

  return serialize().and_then(
    [&]( auto&& serialized ) -> std::expected< multihash, error >
    {
      digest_type digest( 32 );

      if( !secp256k1_vrf_verify( reinterpret_cast< unsigned char* >( digest.data() ),
                                 reinterpret_cast< const unsigned char* >( proof.data() ),
                                 reinterpret_cast< unsigned char* >( serialized.data() ),
                                 input.data(),
                                 input.size() ) )
        return std::unexpected( error_code::reversion ); // "random proof failed verification"

      return multihash( multicodec::sha2_256, std::move( digest ) );
    }
  );
}

public_key& public_key::operator=( const public_key& pk )
{
  _my = std::make_unique< detail::public_key_impl >( *pk._my );
  return *this;
}

public_key& public_key::operator=( public_key&& pk )
{
  _my = std::move( pk._my );
  return *this;
}

bool operator==( const public_key& a, const public_key& b )
{
  return *a._my == *b._my;
}

std::vector< std::byte > public_key::to_address_bytes( std::byte prefix ) const
{
  return _my->to_address_bytes( prefix );
}

std::expected< unsigned int, error > public_key::fingerprint() const
{
  return _my->fingerprint();
}

bool public_key::is_canonical( const recoverable_signature& c )
{
  using boost::multiprecision::uint256_t;
  constexpr uint256_t n_2 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0_cppui256;

  // BIP-0062 states that sig must be in [1,n/2], however because a sig of value 0 is an invalid
  // signature under all circumstances, the lower bound does not need checking
  return std::memcmp( c.data() + 33, &n_2, 32 ) <= 0;
}

private_key::private_key()
{
  _init_lib();
}

private_key::private_key( private_key&& pk ):
    _key( std::move( pk._key ) )
{
  _init_lib();
}

private_key::private_key( const private_key& pk ):
    _key( pk._key )
{
  _init_lib();
}

private_key::~private_key() {}

private_key& private_key::operator=( private_key&& pk )
{
  _key = std::move( pk._key );
  return *this;
}

private_key& private_key::operator=( const private_key& pk )
{
  _key = pk._key;
  return *this;
}

std::expected< private_key, error > private_key::regenerate( const multihash& secret )
{
  if( secret.digest().size() != sizeof( private_key_secret ) )
    return std::unexpected( error_code::reversion ); // "secret must be ${s} bits"

  private_key self;
  std::memcpy( self._key.data(), secret.digest().data(), self._key.size() );
  return self;
}

std::expected< private_key, error > private_key::generate_from_seed( const multihash& seed, const multihash& offset )
{
  // There is non-determinism in this function, perhaps purposefully.
  ssl_bignum z;
  BN_bin2bn( (unsigned char*)offset.digest().data(), offset.digest().size(), z );

  ec_group group( EC_GROUP_new_by_curve_name( NID_secp256k1 ) );
  bn_ctx ctx( BN_CTX_new() );
  ssl_bignum order;
  EC_GROUP_get_order( group, order, ctx );

  // secexp = (seed + z) % order
  ssl_bignum secexp;
  BN_bin2bn( (unsigned char*)&seed, sizeof( seed ), secexp );
  BN_add( secexp, secexp, z );
  BN_mod( secexp, secexp, order, ctx );

  std::vector< std::byte > digest;
  digest.resize( 32 );
  assert( BN_num_bytes( secexp ) <= int64_t( digest.size() ) );
  auto shift = digest.size() - BN_num_bytes( secexp );
  BN_bn2bin( secexp, ( (unsigned char*)digest.data() ) + shift );
  return regenerate( multihash( multicodec::sha2_256, digest ) );
}

private_key_secret private_key::get_secret() const
{
  return _key;
}

std::expected< recoverable_signature, error > private_key::sign_compact( const multihash& digest ) const
{
  if( digest.digest().size() != _key.size() )
    return std::unexpected( error_code::reversion ); // "digest must be ${s} bits"
  if( _key != empty_priv() )
    return std::unexpected( error_code::reversion ); // "cannot sign with an empty key"

  std::array< std::byte, 65 > internal_sig;
  recoverable_signature sig;
  int32_t rec_id;
  unsigned int counter = 0;
  do
  {
    if( !secp256k1_ecdsa_sign_recoverable( _get_context(),
                                           (secp256k1_ecdsa_recoverable_signature*)internal_sig.data(),
                                           (unsigned char*)digest.digest().data(),
                                           (unsigned char*)_key.data(),
                                           extended_nonce_function,
                                           &counter ) )
      return std::unexpected( error_code::reversion ); // "unknown error when signing"

    if( !secp256k1_ecdsa_recoverable_signature_serialize_compact(
          _get_context(),
          (unsigned char*)sig.data() + 1,
          &rec_id,
          (const secp256k1_ecdsa_recoverable_signature*)internal_sig.data() ) )
      return std::unexpected( error_code::reversion ); // "unknown error when serializing recoverable signature"

    sig[ 0 ] = static_cast< std::byte >( rec_id + 31 );
  }
  while( !public_key::is_canonical( sig ) );

  return sig;
}

std::expected< std::pair< std::string, multihash >, error > private_key::generate_random_proof( const std::string& input ) const
{
  return get_public_key().and_then(
    [&]( auto&& pub_key ) -> std::expected< std::pair< std::string, multihash >, error >
    {
      std::string proof( 81, '\0' );

      if( !secp256k1_vrf_prove( reinterpret_cast< unsigned char* >( const_cast< char* >( proof.data() ) ),
                                reinterpret_cast< const unsigned char* >( _key.data() ),
                                reinterpret_cast< secp256k1_pubkey* >( pub_key._my->_key.data() ),
                                input.data(),
                                input.size() ) )
        return std::unexpected( error_code::reversion );

      digest_type digest( 32 );
      if(
        !secp256k1_vrf_proof_to_hash( reinterpret_cast< unsigned char* >( digest.data() ),
                                      reinterpret_cast< unsigned char* >( const_cast< char* >( proof.data() ) ) ) )
        return std::unexpected( error_code::reversion ); // "failed to hash random proof"

      return std::make_pair( std::move( proof ), multihash( multicodec::sha2_256, std::move( digest ) ) );
    }
  );
}

std::expected< public_key, error > private_key::get_public_key() const
{
  if( std::equal( _key.begin(), _key.end(), empty_priv().begin() ) )
    return std::unexpected( error_code::reversion ); // "cannot get private key of an empty public key"

  public_key pk;
  if( !secp256k1_ec_pubkey_create( _get_context(), (secp256k1_pubkey*)pk._my->_key.data(), (unsigned char*)_key.data() ) )
    return std::unexpected( error_code::reversion ); // "unknown error creating public key from a private key"

  return pk;
}

std::string private_key::to_wif( std::byte prefix )
{
  std::array< std::byte, 38 > d;
  uint32_t check;
  assert( _key.size() + sizeof( check ) + 2 == d.size() );
  d[ 0 ] = prefix;
  std::memcpy( d.data() + 1, _key.data(), _key.size() );
  d.data()[ _key.size() + 1 ] = std::byte( 0x01 );
  auto extended_hash          = hash( multicodec::sha2_256, (char*)d.data(), _key.size() + 2 );
  if( !extended_hash )
    throw std::runtime_error( std::string( extended_hash.error().message() ) );

  auto result = hash( multicodec::sha2_256, *extended_hash ).and_then(
    [&]( auto&& hash ) -> std::expected< std::string, error >
    {
      auto check = *( (uint32_t*) hash.digest().data() );
      std::memcpy( d.data() + _key.size() + 2, (const char*)&check, sizeof( check ) );
      return util::encode_base58( d );
    });

  if( !result )
    throw std::runtime_error( std::string( result.error().message() ) );

  return *result;
}

std::expected< private_key, error > private_key::from_wif( const std::string& b58, std::byte prefix )
{
  std::vector< char > d;
  d.reserve( 38 );
  util::decode_base58( b58, d );
  if( d[ 0 ] != std::to_integer< char >( prefix ) )
    return std::unexpected( error_code::reversion ); // "incorrect wif prefix"

  bool compressed = d.size() == 38;
  if( compressed && d[ 33 ] != 0x01 )
    return std::unexpected( error_code::reversion ); // "compressed byte was not 0x01"

  private_key key;
  auto extended_hash = hash( multicodec::sha2_256, d.data(), key._key.size() + ( compressed ? 2 : 1 ) );
  if( !extended_hash )
    return std::unexpected( error_code::reversion );

  return hash( multicodec::sha2_256, *extended_hash ).and_then(
    [&]( auto&& hash ) -> std::expected< private_key, error >
    {
      uint32_t check = *( (uint32_t*)hash.digest().data() );
      if( std::memcmp( (char*)&check, d.data() + key._key.size() + ( compressed ? 2 : 1 ), sizeof( check ) ) )
        return std::unexpected( error_code::reversion ); // "invalid checksum"

      std::memcpy( key._key.data(), d.data() + 1, key._key.size() );
      return key;
    });
}

} // namespace crypto

template<>
void to_binary< crypto::public_key >( std::ostream& s, const crypto::public_key& k )
{
  auto cpk = k.serialize();
  if( !cpk )
    throw std::runtime_error( std::string( cpk.error().message() ) );
  s.write( reinterpret_cast< const char* >( cpk->data() ), cpk->size() );
}

template<>
void from_binary< crypto::public_key >( std::istream& s, crypto::public_key& k )
{
  crypto::compressed_public_key cpk;
  s.read( reinterpret_cast< char* >( cpk.data() ), cpk.size() );
  auto key = crypto::public_key::deserialize( cpk );
  if( !key )
    throw std::runtime_error( std::string( key.error().message() ) );

  k = *key;
}

} // namespace koinos
