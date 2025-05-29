#pragma once

#include "koinos/protocol/types.hpp"
#include <array>
#include <cstddef>
#include <sstream>

#include <boost/archive/binary_oarchive.hpp>

#include <koinos/protocol/protocol.hpp>

namespace koinos::crypto {

constexpr std::size_t digest_length = 32;

using digest = std::array< std::byte, digest_length >;

digest hash( const void* ptr, std::size_t len = 0 );

template< Archivable T >
  requires( !std::is_same_v< T, std::nullptr_t > )
digest hash( T&& t ) noexcept
{
  std::stringstream ss;
  boost::archive::binary_oarchive oa( ss, boost::archive::archive_flags::no_tracking );
  oa << t;
  return hash( static_cast< const void* >( ss.view().data() ), ss.view().size() );
}

void hasher_reset() noexcept;
void hasher_update( const void* ptr, std::size_t len = 0 ) noexcept;

template< Archivable T >
  requires( !std::is_same_v< T, std::nullptr_t > )
void hasher_update( T&& t ) noexcept
{
  std::stringstream ss;
  boost::archive::binary_oarchive oa( ss, boost::archive::archive_flags::no_tracking );
  oa << t;
  hasher_update( static_cast< const void* >( ss.view().data() ), ss.view().size() );
}

digest hasher_finalize() noexcept;

} // namespace koinos::crypto
