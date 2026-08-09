#ifndef UTIL_BYTE_H
#define UTIL_BYTE_H

#include <stdint.h>

uint16_t util_be16_read(const uint8_t *bytes);
void util_be16_write(uint8_t *bytes, uint16_t value);

#endif
