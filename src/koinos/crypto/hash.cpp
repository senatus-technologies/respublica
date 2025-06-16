#include <cstdint>
#include <cstring>
#include <koinos/crypto/hash.hpp>
#include <koinos/memory/memory.hpp>

#include <blake3.h>

namespace koinos::crypto {

namespace detail {

struct blake3
{
  blake3_hasher hasher{};

  blake3()
  {
    blake3_hasher_init( &hasher );
  }
};
} // namespace detail

// NOLINTBEGIN
thread_local static detail::blake3 blake3;

// NOLINTEND

digest hash( const void* ptr, std::size_t len )

{
  digest out;
  blake3_hasher_reset( &blake3.hasher );
  blake3_hasher_update( &blake3.hasher, ptr, len );
  blake3_hasher_finalize( &blake3.hasher, memory::pointer_cast< std::uint8_t* >( out.data() ), out.size() );
  return out;
}

digest hash( const char* s ) noexcept
{
  return hash( s, std::strlen( s ) );
}

digest hash( const std::string& s ) noexcept
{
  return hash( s.data(), s.size() );
}

digest hash( std::string_view sv ) noexcept
{
  return hash( sv.data(), sv.size() );
}

void hasher_reset() noexcept
{
  blake3_hasher_reset( &blake3.hasher );
}

void hasher_update( const void* ptr, std::size_t len ) noexcept
{
  blake3_hasher_update( &blake3.hasher, ptr, len );
}

void hasher_update( const char* s ) noexcept
{
  blake3_hasher_update( &blake3.hasher, static_cast< const void* >( s ), std::strlen( s ) );
}

void hasher_update( const std::string& s ) noexcept
{
  blake3_hasher_update( &blake3.hasher, static_cast< const void* >( s.data() ), s.size() );
}

void hasher_update( std::string_view sv ) noexcept
{
  blake3_hasher_update( &blake3.hasher, static_cast< const void* >( sv.data() ), sv.size() );
}

digest hasher_finalize() noexcept
{
  digest out;
  blake3_hasher_finalize( &blake3.hasher, memory::pointer_cast< std::uint8_t* >( out.data() ), out.size() );
  return out;
}

} // namespace koinos::crypto
