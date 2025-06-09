#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace koinos::crypto {

constexpr std::size_t digest_length = 32;

using digest = std::array< std::byte, digest_length >;

void hasher_reset() noexcept;
digest hasher_finalize() noexcept;
void hasher_update( const void* ptr, std::size_t len = 0 ) noexcept;
void hasher_update( const char* s ) noexcept;
void hasher_update( const std::string& s ) noexcept;
void hasher_update( std::string_view sv ) noexcept;

template< typename T >
  requires( std::is_same_v< T, std::nullptr_t > )
void hasher_update( T& t ) noexcept
{
  hasher_update( nullptr, 0 );
}

template< typename T >
  requires( std::is_trivially_copyable_v< typename std::remove_reference< T >::type >
            && !std::is_integral_v< typename std::remove_reference< T >::type > )
void hasher_update( T&& t ) noexcept
{
  hasher_update( &t, sizeof( T ) );
}

template< typename T >
  requires std::is_integral_v< T >
void hasher_update( T t ) noexcept
{
  if constexpr( std::endian::native != std::endian::little )
    t = std::byteswap( t );

  hasher_update( &t, sizeof( T ) );
}

template< typename Range >
  requires( std::ranges::range< Range >
            && !std::is_trivially_copyable_v< typename std::remove_reference< Range >::type > )
void hasher_update( Range&& values ) noexcept
{
  for( const auto& value: values )
    hasher_update( value );
}

template< typename T >
void hasher_update( std::span< T > s ) noexcept
{
  hasher_update( s.data(), s.size_bytes() );
}

digest hash( const void* ptr, std::size_t len = 0 );
digest hash( const char* s ) noexcept;
digest hash( const std::string& s ) noexcept;
digest hash( std::string_view sv ) noexcept;

template< typename T >
  requires( std::is_same_v< T, std::nullptr_t > )
digest hash( T& t ) noexcept
{
  return hash( nullptr, 0 );
}

template< typename T >
  requires( std::is_trivially_copyable_v< typename std::remove_reference< T >::type >
            && !std::is_integral_v< typename std::remove_reference< T >::type > )
digest hash( T&& t ) noexcept
{
  return hash( &t, sizeof( T ) );
}

template< typename T >
  requires std::is_integral_v< T >
digest hash( T t ) noexcept
{
  if constexpr( std::endian::native != std::endian::little )
    t = std::byteswap( t );

  return hash( &t, sizeof( T ) );
}

template< typename Range >
  requires( std::ranges::range< Range >
            && !std::is_trivially_copyable_v< typename std::remove_reference< Range >::type > )
digest hash( Range&& values ) noexcept
{
  hasher_reset();
  for( const auto& value: values )
    hasher_update( value );
  return hasher_finalize();
}

template< typename T >
digest hash( std::span< T > s ) noexcept
{
  return hash( s.data(), s.size_bytes() );
}

} // namespace koinos::crypto
