#ifndef UTIL_BYTE_H
#define UTIL_BYTE_H

#include <stdint.h>

#include "util_status.h"

sns_status_t util_be16_read(const uint8_t *bytes, uint16_t capacity, uint16_t *value);
sns_status_t util_be16_write(uint8_t *bytes, uint16_t capacity, uint16_t value);

#endif
