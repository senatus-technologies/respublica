#include <koinos/crypto/hash.hpp>

#include <blake3.h>

namespace koinos::crypto {

namespace detail {

struct blake3
{
  blake3_hasher hasher;

  blake3()
  {
    blake3_hasher_init( &hasher );
  }
};
} // namespace detail

thread_local static detail::blake3 blake3;

digest hash( const void* ptr, std::size_t len )

{
  digest out;
  blake3_hasher_reset( &blake3.hasher );
  blake3_hasher_update( &blake3.hasher, ptr, len );
  blake3_hasher_finalize( &blake3.hasher, reinterpret_cast< uint8_t* >( out.data() ), out.size() );
  return out;
}

void hasher_reset() noexcept
{
  blake3_hasher_reset( &blake3.hasher );
}

void hasher_update( const void* ptr, std::size_t len ) noexcept
{
  blake3_hasher_update( &blake3.hasher, ptr, len );
}

digest hasher_finalize() noexcept
{
  digest out;
  blake3_hasher_finalize( &blake3.hasher, reinterpret_cast< uint8_t* >( out.data() ), out.size() );
  return out;
}

} // namespace koinos::crypto
