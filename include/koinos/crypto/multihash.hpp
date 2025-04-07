#pragma once

#include <cstddef>
#include <ostream>
#include <sstream>

#include <openssl/evp.h>

#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>

#include <koinos/exception.hpp>
#include <koinos/varint.hpp>

namespace google::protobuf {
class Message;
} // namespace google::protobuf

namespace koinos {

namespace crypto {
class multihash;
} // namespace crypto

template<>
void to_binary< crypto::multihash >( std::ostream& s, const crypto::multihash& m );

template<>
void from_binary< crypto::multihash >( std::istream& s, crypto::multihash& v );

template<>
void to_binary< std::string >( std::ostream& s, const std::string& v );

template< class T >
std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void >
to_binary( std::ostream& s, const google::protobuf::RepeatedPtrField< T >& rpf )
{
  for( const auto& t: rpf )
  {
    t.SerializeToOstream( s );
  }
}

template< class T >
std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T >, void >
to_binary( std::ostream& s, const google::protobuf::RepeatedPtrField< T >& rpf )
{
  for( const auto& t: rpf )
  {
    to_binary( s, t );
  }
}

namespace crypto {

KOINOS_DECLARE_EXCEPTION( unknown_hash_algorithm );
KOINOS_DECLARE_EXCEPTION( multihash_size_mismatch );
KOINOS_DECLARE_EXCEPTION( multihash_size_limit_exceeded );
KOINOS_DECLARE_EXCEPTION( internal_error );

/*
 * Multicodec IDs for hash algorithms
 * https://github.com/multiformats/multicodec/blob/master/table.csv
 */
enum class multicodec : std::uint64_t
{
  identity   = 0x00,
  sha1       = 0x11,
  sha2_256   = 0x12,
  sha2_512   = 0x13,
  keccak_256 = 0x1b,
  ripemd_160 = 0x1053
};

using digest_type = std::vector< std::byte >;

enum class digest_size : std::size_t
{
};

class multihash
{
public:
  multihash() = default;
  multihash( multicodec code, const digest_type& digest );
  multihash( multicodec code, digest_type&& digest );

  multicodec code() const;
  const digest_type& digest() const;
  bool is_zero() const;

  static digest_size standard_size( multicodec id );
  static multihash zero( multicodec id, digest_size size = digest_size( 0 ) );
  static multihash empty( multicodec id, digest_size size = digest_size( 0 ) );

  multihash& operator=( const multihash& rhs );

  bool operator==( const multihash& rhs ) const;
  bool operator!=( const multihash& rhs ) const;
  bool operator<( const multihash& rhs ) const;
  bool operator<=( const multihash& rhs ) const;
  bool operator>( const multihash& rhs ) const;
  bool operator>=( const multihash& rhs ) const;

  friend std::ostream& operator<<( std::ostream&, const multihash& );

private:
  multicodec _code = multicodec::identity;
  digest_type _digest;
};

namespace detail {

struct encoder: std::streambuf,
                std::ostream
{
  encoder( multicodec code, std::size_t size = 0 );
  encoder( const encoder& ) = delete;
  encoder( encoder&& )      = delete;
  virtual ~encoder()        = default;

  // These are the streambuf and ostream functions needed to be implemented
  //   std::streamsize xsputn( const char* s, std::streamsize n ) override;
  //   void reset();

  void write( const char* d, std::size_t len );
  void put( char c );
  void set_size( std::size_t size = 0 );
  virtual multihash get_hash() = 0;

protected:
  multicodec _code;
  std::size_t _size;
};

struct openssl_encoder final: encoder
{
  openssl_encoder( multicodec code, std::size_t size = 0 );
  openssl_encoder( const openssl_encoder& ) = delete;
  openssl_encoder( openssl_encoder&& )      = delete;
  ~openssl_encoder();

  std::streamsize xsputn( const char* s, std::streamsize n ) override;
  void reset();

  virtual multihash get_hash() override;

private:
  const EVP_MD* md  = nullptr;
  EVP_MD_CTX* mdctx = nullptr;
};

struct ethash_encoder final: encoder
{
  ethash_encoder( multicodec code, std::size_t size = 0 );
  ethash_encoder( const ethash_encoder& ) = delete;
  ethash_encoder( ethash_encoder&& )      = delete;
  ~ethash_encoder()                       = default;

  std::streamsize xsputn( const char* s, std::streamsize n ) override;
  void reset();

  multihash get_hash() override;

private:
  std::stringbuf _buf;
};

void hash_c_str( encoder& e, const char* data, std::size_t len );
void hash_bytes( encoder& e, const std::vector< std::byte >& d );
void hash_str( encoder& e, const std::string& s );
void hash_multihash( encoder& e, const multihash& m );

template< class T >
std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void > hash_impl( encoder& e, const T& t )
{
  t.SerializeToOstream( &e );
}

template< class T >
std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void > hash_impl( encoder& e, const T* t )
{
  t->SerializeToOstream( &e );
}

template< class T >
std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void > hash_impl( encoder& e, T* t )
{
  t->SerializeToOstream( &e );
}

template< class T >
std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T >, void > hash_impl( encoder& e, const T& t )
{
  to_binary( e, t );
}

template< class T >
std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T >, void > hash_impl( encoder& e, const T* t )
{
  to_binary( e, *t );
}

template< class T >
std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T >, void > hash_impl( encoder& e, T* t )
{
  to_binary( e, *t );
}

inline void hash_n_impl( encoder& e ) {} // Base cases for recursive templating

inline void hash_n_impl( encoder& e, digest_size size )
{
  e.set_size( std::size_t( size ) );
}

template< class... Ts >
void hash_n_impl( encoder& e, const char* data, std::size_t len, Ts... ts )
{
  hash_c_str( e, data, len );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, char* data, std::size_t len, Ts... ts )
{
  hash_c_str( e, data, len );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, const multihash& m, Ts... ts )
{
  hash_multihash( e, m );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, multihash&& m, Ts... ts )
{
  hash_multihash( e, m );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, const std::vector< std::byte >& d, Ts... ts )
{
  hash_bytes( e, d );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, std::vector< std::byte >&& d, Ts... ts )
{
  hash_bytes( e, d );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, const std::string& s, Ts... ts )
{
  hash_str( e, s );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class... Ts >
void hash_n_impl( encoder& e, std::string&& s, Ts... ts )
{
  hash_str( e, s );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

template< class T, class... Ts >
void hash_n_impl( encoder& e, T&& t, Ts... ts )
{
  hash_impl( e, t );
  hash_n_impl( e, std::forward< Ts >( ts )... );
}

} // namespace detail

/*
 * hash() hashes a series of objects in to a single hash.
 *
 * Types currently supported:
 *
 * - std::string
 * - std::vector< std::byte >
 * - C string (const char*, size_t)
 * - Protobuf generated types (google::protobuf::Message)
 * - Types implementing `to_binary( std::ostream&, const T& )`
 *
 * If the last parameter is digest_size, a custom hash size will be used.
 * Effectively, the function's signature is hash( multicodec code, Ts... ts, size_t size = 0 )
 */
template< class... Ts >
multihash hash( multicodec code, Ts... ts )
{
  switch( code )
  {
    case multicodec::sha1:
      [[fallthrough]];
    case multicodec::sha2_256:
      [[fallthrough]];
    case multicodec::sha2_512:
      [[fallthrough]];
    case multicodec::ripemd_160:
      {
        detail::openssl_encoder e( code );
        detail::hash_n_impl( e, std::forward< Ts >( ts )... );
        return e.get_hash();
        break;
      }
    case multicodec::keccak_256:
      {
        detail::ethash_encoder e( code );
        detail::hash_n_impl( e, std::forward< Ts >( ts )... );
        return e.get_hash();
        break;
      }
    default:
      KOINOS_ASSERT( false,
                     unknown_hash_algorithm,
                     "unknown hash code ${i}",
                     ( "i", static_cast< std::underlying_type_t< multicodec > >( code ) ) );
  }
}

std::ostream& operator<<( std::ostream&, const crypto::multihash& );

void to_json( nlohmann::json&, const multihash& );

} // namespace crypto

} // namespace koinos
