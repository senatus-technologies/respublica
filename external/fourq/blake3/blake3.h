#ifndef __BLAKE3_H__
#define __BLAKE3_H__

// For C++
#ifdef __cplusplus
extern "C"
{
#endif

  // Hashing using blake3. Output is 64 bytes long
  int crypto_blake3( const unsigned char* in, unsigned long long inlen, unsigned char* out );

#ifdef __cplusplus
}
#endif

#endif
