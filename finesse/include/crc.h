
#ifndef __CRC_H__
#define __CRC_H__ (1)
#include <stddef.h>
#include <stdint.h>

uint32_t crc32c(uint32_t crc, const void *buf, size_t len);

#endif  // __CRC_H__
