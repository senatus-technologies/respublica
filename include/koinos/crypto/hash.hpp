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

digest hash( const void* ptr, std::size_t len );

template< Archivable T >
digest hash( T&& t )
{
  std::stringstream ss;
  boost::archive::binary_oarchive oa( ss, boost::archive::archive_flags::no_tracking );
  oa << t;
  return hash( static_cast< const void* >( ss.view().data() ), ss.view().size() );
}

} // namespace koinos::crypto
