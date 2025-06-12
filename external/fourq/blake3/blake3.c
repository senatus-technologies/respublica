#include "blake3.h"

#include <blake3.h>

int crypto_blake3( const unsigned char* in, unsigned long long inlen, unsigned char* out )
{
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );
  blake3_hasher_update( &hasher, in, inlen );
  blake3_hasher_finalize( &hasher, out, 64 );
  return 0;
}
