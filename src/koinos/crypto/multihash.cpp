#include <koinos/crypto/multihash.hpp>

#include <koinos/util/base64.hpp>
#include <koinos/util/hex.hpp>

#include <ethash/keccak.hpp>

#include <iostream>
#include <map>

namespace koinos {
namespace crypto {

using koinos::error::error_code;

multihash::multihash( multicodec code, const digest_type& digest ):
    _code( code ),
    _digest( digest )
{}

multihash::multihash( multicodec code, digest_type&& digest ):
    _code( code ),
    _digest( digest )
{}

multicodec multihash::code() const
{
  return _code;
}

const digest_type& multihash::digest() const
{
  return _digest;
}

std::expected< digest_size, error > multihash::standard_size( multicodec id )
{
  switch( id )
  {
    case multicodec::sha1:
      return digest_size( 20 );
    case multicodec::sha2_256:
      return digest_size( 32 );
    case multicodec::sha2_512:
      return digest_size( 64 );
    case multicodec::keccak_256:
      return digest_size( 32 );
    case multicodec::ripemd_160:
      return digest_size( 20 );
    default:
      return std::unexpected( error_code::reversion ); // "unknown hash code ${i}"
  }
}

std::expected< multihash, error > multihash::zero( multicodec code, digest_size size )
{
  digest_type result;

  if( size == digest_size( 0 ) )
  {
    if( auto default_size = multihash::standard_size( code ); default_size )
      size = default_size.value();
    else
      return std::unexpected( default_size.error() );
  }

  result.resize( std::size_t( size ) );
  std::memset( result.data(), 0, std::size_t( size ) );
  return multihash( code, result );
}

std::expected< multihash, error > multihash::empty( multicodec code, digest_size size )
{
  char c;
  return hash( code, &c, std::size_t( 0 ), size );
}

bool multihash::is_zero() const
{
  return std::all_of( _digest.begin(),
                      _digest.end(),
                      []( std::byte b )
                      {
                        return b == std::byte{ 0x00 };
                      } );
}

multihash& multihash::operator=( const multihash& rhs )
{
  _code = rhs._code;

  _digest.resize( rhs._digest.size() );
  std::copy( rhs._digest.begin(), rhs._digest.end(), _digest.begin() );

  return *this;
}

bool multihash::operator==( const multihash& rhs ) const
{
  return _code == rhs._code && _digest == rhs._digest;
}

bool multihash::operator!=( const multihash& rhs ) const
{
  return !( *this == rhs );
}

bool multihash::operator<( const multihash& rhs ) const
{
  if( _code != rhs._code )
    return _code < rhs._code;

  return _digest < rhs._digest;
}

bool multihash::operator>( const multihash& rhs ) const
{
  if( _code != rhs._code )
    return _code > rhs._code;

  return _digest > rhs._digest;
}

bool multihash::operator<=( const multihash& rhs ) const
{
  return *this < rhs || *this == rhs;
}

bool multihash::operator>=( const multihash& rhs ) const
{
  return *this > rhs || *this == rhs;
}

namespace detail {

encoder::encoder( multicodec code, std::size_t size ):
    std::streambuf(),
    std::ostream( this ),
    _code( code )
{
  set_size( size );
}

void encoder::write( const char* d, size_t len )
{
  xsputn( d, len );
}

void encoder::put( char c )
{
  xsputn( &c, 1 );
}

error encoder::set_size( std::size_t size )
{
  static const uint64_t MAX_HASH_SIZE = std::min< uint64_t >( {
    std::numeric_limits< uint8_t >::max(),      // We potentially store the size in uint8_t value
    std::numeric_limits< unsigned int >::max(), // We cast the size to unsigned int for openssl call
    EVP_MAX_MD_SIZE                             // Max size supported by OpenSSL library
  } );

  std::size_t default_size;

  if( auto size = multihash::standard_size( _code ); size )
    default_size = std::size_t( size.value() );
  else
    return size.error();

  if( size == 0 )
    size = default_size;
  else if( size > default_size )
    return error(
      error_code::reversion ); // "requested hash size ${size} is larger than max size ${max} for algorithm ${algo}"

  if( size > MAX_HASH_SIZE )
    return error( error_code::reversion ); // "requested hash size ${size} is larger than max size ${max}"

  _size = size;

  return {};
}

const EVP_MD* get_evp_md( multicodec code )
{
  static const std::map< multicodec, const EVP_MD* > evp_md_map = {
    {      multicodec::sha1,      EVP_sha1()},
    {  multicodec::sha2_256,    EVP_sha256()},
    {  multicodec::sha2_512,    EVP_sha512()},
    {multicodec::ripemd_160, EVP_ripemd160()}
  };

  auto md_itr = evp_md_map.find( code );
  return md_itr != evp_md_map.end() ? md_itr->second : nullptr;
}

openssl_encoder::openssl_encoder( multicodec code, std::size_t size ):
    encoder( code, size )
{
  OpenSSL_add_all_digests();
  md = get_evp_md( code );
  if( !md )
    throw std::runtime_error(
      "unknown openssl hash id" ); // We need to ensure we check hash IDs before getting to this point

  mdctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex( mdctx, md, NULL );
}

openssl_encoder::~openssl_encoder()
{
  if( mdctx )
    EVP_MD_CTX_destroy( mdctx );
}

std::streamsize openssl_encoder::xsputn( const char* d, std::streamsize n )
{
  EVP_DigestUpdate( mdctx, d, n );
  pbump( n );
  return n;
}

void openssl_encoder::reset()
{
  if( mdctx )
    EVP_MD_CTX_destroy( mdctx );

  if( md )
  {
    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex( mdctx, md, NULL );
  }
}

multihash openssl_encoder::get_hash()
{
  unsigned int size = (unsigned int)_size;
  std::vector< std::byte > v( size );

  if( !EVP_DigestFinal_ex( mdctx, (unsigned char*)( v.data() ), &size ) )
    throw std::runtime_error( "EVP_DigestFinal_ex returned failure" );

  if( size != _size )
    throw std::runtime_error( "OpenSSL EVP_DigestFinal_ex returned hash size does not match expected hash size" );

  return multihash( _code, std::move( v ) );
}

ethash_encoder::ethash_encoder( multicodec code, std::size_t size ):
    encoder( code, size )
{}

std::streamsize ethash_encoder::xsputn( const char* d, std::streamsize n )
{
  return _buf.sputn( d, n );
}

void ethash_encoder::reset()
{
  _buf = std::stringbuf();
}

multihash ethash_encoder::get_hash()
{
  if( _code == multicodec::keccak_256 )
    throw std::runtime_error( "incorrect ethash_encoder hash code" );

  std::vector< std::byte > v;
  v.reserve( _size );
  auto data_str = _buf.str();

  auto hash_256 = ethash::keccak256( reinterpret_cast< const uint8_t* >( data_str.c_str() ), data_str.size() );
  v.insert( v.end(),
            reinterpret_cast< std::byte* >( hash_256.str ),
            reinterpret_cast< std::byte* >( hash_256.str + _size ) );

  return multihash( _code, std::move( v ) );
}

void hash_bytes( encoder& e, const std::vector< std::byte >& d )
{
  hash_c_str( e, reinterpret_cast< const char* >( d.data() ), d.size() );
}

void hash_str( encoder& e, const std::string& s )
{
  hash_c_str( e, s.data(), s.size() );
}

void hash_c_str( encoder& e, const char* data, std::size_t len )
{
  e.write( data, len );
}

void hash_multihash( encoder& e, const multihash& m )
{
  hash_c_str( e, (const char*)m.digest().data(), m.digest().size() );
}

} // namespace detail

std::ostream& operator<<( std::ostream& out, const crypto::multihash& mh )
{
  std::stringstream bin;
  to_binary( bin, mh );
  return out << util::to_hex( bin.str() );
}

void to_json( nlohmann::json& j, const multihash& mh )
{
  std::stringstream bin;
  to_binary( bin, mh );
  std::string base64 = util::to_base64( bin.str() );
  j                  = base64;
}

} // namespace crypto

template<>
void to_binary< crypto::multihash >( std::ostream& s, const crypto::multihash& m )
{
  auto code = unsigned_varint( static_cast< std::underlying_type_t< crypto::multicodec > >( m.code() ) );
  auto size = unsigned_varint( m.digest().size() );

  to_binary( s, code );
  to_binary( s, size );
  s.write( reinterpret_cast< const char* >( m.digest().data() ), m.digest().size() );
}

template<>
void from_binary< crypto::multihash >( std::istream& s, crypto::multihash& v )
{
  unsigned_varint code;
  unsigned_varint size;
  crypto::digest_type digest;

  from_binary( s, code );
  from_binary( s, size );

  digest.resize( size.value );
  s.read( reinterpret_cast< char* >( digest.data() ), size.value );

  v = crypto::multihash( static_cast< crypto::multicodec >( code.value ), digest );
}

template<>
void to_binary< std::string >( std::ostream& s, const std::string& v )
{
  s.write( v.c_str(), v.size() );
}

} // namespace koinos
