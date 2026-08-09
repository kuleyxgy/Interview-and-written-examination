#include "util_byte.h"

#include <stddef.h>

sns_status_t util_be16_read(const uint8_t *bytes, uint16_t capacity, uint16_t *value)
{
    uint16_t decoded;

    if ((bytes == NULL) || (capacity < 2U) || (value == NULL)) {
        return SNS_ERR_PARAM;
    }

    decoded = (uint16_t)(((uint16_t)bytes[0] << 8U) | (uint16_t)bytes[1]);
    *value = decoded;

    return SNS_OK;
}

sns_status_t util_be16_write(uint8_t *bytes, uint16_t capacity, uint16_t value)
{
    if ((bytes == NULL) || (capacity < 2U)) {
        return SNS_ERR_PARAM;
    }

    bytes[0] = (uint8_t)(value >> 8U);
    bytes[1] = (uint8_t)value;

    return SNS_OK;
}
