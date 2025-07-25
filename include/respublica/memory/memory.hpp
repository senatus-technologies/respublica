#pragma once

#include <array>
#include <cstring>
#include <new>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace respublica::memory {

template< typename T, typename U >
  requires( std::is_same_v< T, void* >
            || (std::is_pointer_v< T > && std::is_trivially_copyable_v< std::remove_pointer_t< T > >))
T pointer_cast( U* p )
{
  return reinterpret_cast< T >( p ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template< typename T >
  requires( std::is_trivially_copyable_v< T > )
inline T* start_lifetime_as( void* p ) noexcept
{
  return std::launder( static_cast< T* >( std::memmove( p, p, sizeof( T ) ) ) );
}

template< typename T, typename U >
  requires( std::is_trivially_copyable_v< T > && std::is_trivially_copyable_v< U > )
inline T* start_lifetime_as( U* u )
{
  return start_lifetime_as< T >( static_cast< void* >( u ) );
}

template< typename T >
  requires( std::is_trivially_copyable_v< T > )
inline T* start_lifetime_as( const void* p ) noexcept
{
  return std::launder( static_cast< const T* >(
    std::memmove( const_cast< void* >( p ), p, sizeof( T ) ) ) ); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

template< typename T, typename U >
  requires( std::is_trivially_copyable_v< T > && std::is_trivially_copyable_v< U > )
inline T* start_lifetime_as( const U* u )
{
  return start_lifetime_as< T >( static_cast< const void* >( u ) );
}

template< typename T >
  requires( !std::is_pointer_v< T > && std::is_trivially_copyable_v< T > )
inline T bit_cast( std::span< const std::byte > bytes )
{
  if( bytes.size() < sizeof( T ) )
    throw std::runtime_error( "byte span is too small" );

  T t;
  std::memcpy( &t, bytes.data(), sizeof( T ) );
  return t;
}

template< std::ranges::range T >
std::span< const std::byte > as_bytes( const T& t )
{
  return std::as_bytes( std::span( t ) );
}

template< typename T, std::size_t N >
  requires( std::is_trivially_copyable_v< T > )
inline std::span< const std::byte > as_bytes( const std::array< T, N >& a )
{
  return std::as_bytes( std::span< const T, std::dynamic_extent >( a.data(), a.size() ) );
}

inline std::span< const std::byte > as_bytes( const std::string& s )
{
  return std::as_bytes( std::span( s ) );
}

inline std::span< const std::byte > as_bytes( const std::string_view& sv )
{
  return std::as_bytes( std::span( sv ) );
}

template< typename T >
  requires( !std::ranges::range< T > && std::is_trivially_copyable_v< T > )
inline std::span< const std::byte > as_bytes( const T& t )
{
  return std::as_bytes( std::span( std::addressof( t ), 1 ) );
}

template< typename T >
  requires( std::is_trivially_copyable_v< T > )
inline std::span< const std::byte > as_bytes( const T* ptr, std::size_t len )
{
  return std::as_bytes( std::span( ptr, len ) );
}

template< typename T >
  requires std::is_trivially_copyable_v< T >
inline std::span< std::byte > as_writable_bytes( T& t )
{
  return std::as_writable_bytes( std::span( std::addressof( t ), 1 ) );
}

template< typename T >
  requires( std::is_trivially_copyable_v< T > )
inline std::span< std::byte > as_writable_bytes( T* ptr, std::size_t len )
{
  return std::as_writable_bytes( std::span( ptr, len ) );
}

} // namespace respublica::memory
