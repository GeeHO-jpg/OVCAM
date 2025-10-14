#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32_init(void);                               // = 0xFFFFFFFF
uint32_t crc32_update(uint32_t crc, const void* data, size_t len);
uint32_t crc32_final(uint32_t crc);                      // return ~crc
uint32_t crc32_calc(const void* data, size_t len);       // สะดวก: init+update+final

#ifdef __cplusplus
}
#endif