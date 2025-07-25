// NOLINTBEGIN( cppcoreguidelines-* )

#include <respublica/encode/base58.hpp>
#include <respublica/memory.hpp>

#include <cassert>
#include <cstdint>

// Base58 algorithm is adapted from Bitcoin's implementation
// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license
// http://www.opensource.org/licenses/mit-license.php.

namespace respublica::encode {

using namespace std::string_literals;

constexpr std::size_t max_array_size = 1'024 * 1'024 * 10;

/** All alphanumeric characters except for "0", "I", "O", and "l" */
static const auto pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"s;
constexpr std::array< std::int8_t, 256 > mapBase58{
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,
  -1, -1, -1, -1, -1, -1, -1, 9,  10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1, 22, 23, 24, 25, 26, 27, 28,
  29, 30, 31, 32, -1, -1, -1, -1, -1, -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46, 47, 48, 49, 50,
  51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

constexpr bool is_space( char c ) noexcept
{
  return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

std::error_code decode_base58( const char* psz,
                               std::size_t size,
                               std::vector< std::byte >& dest,
                               std::size_t max_ret_len = max_array_size )
{
  auto end = psz + size;

  // Skip leading spaces.
  while( psz != end && is_space( *psz ) )
    psz++;
  // Skip and count leading '1's.
  int zeroes = 0;
  int length = 0;
  while( *psz == '1' )
  {
    zeroes++;
    if( zeroes > max_ret_len )
      return encode_errc::invalid_length;
    psz++;
  }

  // Allocate enough space in big-endian base256 representation.
  size = size * 733 / 1'000 + 1; // log(58) / log(256), rounded up.
  std::vector< unsigned char > b256( size );
  // Process the characters.
  static_assert( sizeof( mapBase58 ) / sizeof( mapBase58.at( 0 ) ) == 256,
                 "mapBase58.size() should be 256" ); // guarantee not out of range
  while( psz != end && !is_space( *psz ) )
  {
    // Decode base58 character
    int carry = mapBase58.at( (std::uint8_t)*psz );
    if( carry == -1 ) // Invalid b58 character
      return encode_errc::invalid_character;
    int i = 0;
    for( std::vector< unsigned char >::reverse_iterator it = b256.rbegin();
         ( carry != 0 || i < length ) && ( it != b256.rend() );
         ++it, ++i )
    {
      carry += 58 * ( *it );
      *it    = carry % 256;
      carry /= 256;
    }
    assert( carry == 0 );
    length = i;
    if( length + zeroes > max_ret_len )
      return encode_errc::invalid_length;
    psz++;
  }
  // Skip trailing spaces.
  while( psz != end && is_space( *psz ) )
    psz++;
  if( psz != end )
    return encode_errc::invalid_length;
  // Skip leading zeroes in b256.
  std::vector< unsigned char >::iterator it = b256.begin() + ( size - length );
  // Copy result into output vector.
  dest.reserve( zeroes + ( b256.end() - it ) );
  dest.assign( zeroes, std::byte{ 0x00 } );
  while( it != b256.end() )
    dest.push_back( std::byte{ *( it++ ) } );
  return encode_errc::ok;
}

std::string encode_base58( const unsigned char* pbegin, const unsigned char* pend )
{
  // Skip & count leading zeroes.
  int zeroes = 0;
  int length = 0;
  while( pbegin != pend && *pbegin == 0 )
  {
    pbegin++;
    zeroes++;
  }
  // Allocate enough space in big-endian base58 representation.
  int size = ( pend - pbegin ) * 138 / 100 + 1; // log(256) / log(58), rounded up.
  std::vector< unsigned char > b58( size );
  // Process the bytes.
  while( pbegin != pend )
  {
    int carry = *pbegin;
    int i     = 0;
    // Apply "b58 = b58 * 256 + ch".
    for( std::vector< unsigned char >::reverse_iterator it = b58.rbegin();
         ( carry != 0 || i < length ) && ( it != b58.rend() );
         it++, i++ )
    {
      carry += 256 * ( *it );
      *it    = carry % 58;
      carry /= 58;
    }

    assert( carry == 0 );
    length = i;
    pbegin++;
  }
  // Skip leading zeroes in base58 result.
  std::vector< unsigned char >::iterator it = b58.begin() + ( size - length );
  while( it != b58.end() && *it == 0 )
    it++;
  // Translate the result into a string.
  std::string str;
  str.reserve( zeroes + ( b58.end() - it ) );
  str.assign( zeroes, '1' );
  while( it != b58.end() )
    str += pszBase58[ *( it++ ) ];
  return str;
}

std::string to_base58( std::span< const std::byte > s ) noexcept
{
  return encode_base58( memory::pointer_cast< const unsigned char* >( s.data() ),
                        memory::pointer_cast< const unsigned char* >( s.data() + s.size() ) );
}

result< std::vector< std::byte > > from_base58( std::string_view sv ) noexcept
{
  std::vector< std::byte > dest;
  if( auto error = decode_base58( &sv.front(), sv.size(), dest ); error )
    return std::unexpected( error );

  return dest;
}

} // namespace respublica::encode

// NOLINTEND( cppcoreguidelines-* )
