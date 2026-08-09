#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>

#include "util_status.h"

typedef struct hal_i2c hal_i2c_t;

typedef struct {
    sns_status_t (*transfer)(void *ctx,
                             uint16_t address,
                             const uint8_t *tx,
                             uint16_t tx_len,
                             uint8_t *rx,
                             uint16_t rx_capacity,
                             uint32_t timeout_ms,
                             uint16_t *transferred);
} hal_i2c_ops_t;

struct hal_i2c {
    const hal_i2c_ops_t *ops;
    void *ctx;
};

#endif
