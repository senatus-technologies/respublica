#pragma once

#include <cstring>
#include <type_traits>

namespace koinos::util {

template< typename T, typename U >
  requires( std::is_same_v< T, void* > ||
            ( std::is_pointer_v< T > &&
              std::is_trivially_constructible_v< std::remove_pointer_t< T > > &&
              std::is_trivially_copyable_v< std::remove_pointer_t< T > > ) )
T pointer_cast( U* p )
{
  return reinterpret_cast< T >( p );
}

template< typename T >
  requires( std::is_trivially_copyable_v< T > &&
            std::is_trivially_constructible_v< T > )
inline T* start_lifetime_as( void* p ) noexcept
{
  return std::launder( static_cast< T* >( std::memmove( p, p, sizeof( T ) ) ) );
}

template< typename T, typename U >
  requires( std::is_trivially_copyable_v< T > &&
            std::is_trivially_constructible_v< T > &&
            std::is_trivially_copyable_v< U > &&
            std::is_trivially_constructible_v< U > )
inline T* start_lifetime_as( U* u )
{
  return start_lifetime_as< T >( static_cast< void* >( u ) );
}

template< typename T >
  requires( std::is_trivially_copyable_v< T > &&
            std::is_trivially_constructible_v< T > )
inline T* start_lifetime_as( const void* p ) noexcept
{
  return std::launder( static_cast< const T* >( std::memmove( const_cast< void* >( p ), p, sizeof( T ) ) ) );
}

template< typename T, typename U >
  requires( std::is_trivially_copyable_v< T > &&
            std::is_trivially_constructible_v< T > &&
            std::is_trivially_copyable_v< U > &&
            std::is_trivially_constructible_v< U > )
inline T* start_lifetime_as( const U* u )
{
  return start_lifetime_as< T >( static_cast< const void* >( u ) );
}

} // namespace koinos::util
