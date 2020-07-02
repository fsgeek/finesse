

#if !defined(__MURMURHASH3_H__)
#define __MURMURHASH3_H__ (1)

#include <stdint.h>

void MurmurHash3_x86_32 ( const void * key, int len,
                          uint32_t seed, void * out );

void MurmurHash3_x86_128 ( const void * key, const int len,
                           uint32_t seed, void * out );

void MurmurHash3_x64_128 ( const void * key, const int len,
                           const uint32_t seed, void * out );                           

#endif // __MURMUR_HASH3_H__
