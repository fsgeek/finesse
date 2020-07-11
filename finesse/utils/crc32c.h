
#if !defined(__CRC32C_H__)
#define __CRC32C_H__

#include <stdint.h>
#include <stdlib.h>

uint32_t crc32c(uint32_t crc, const void *buf, size_t len);

#endif // __CRC32C_H__
