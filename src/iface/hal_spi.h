#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

#include "util_status.h"

typedef struct hal_spi hal_spi_t;

typedef struct {
    sns_status_t (*transfer)(void *ctx,
                             const uint8_t *tx,
                             uint8_t *rx,
                             uint16_t length,
                             uint32_t timeout_ms,
                             uint16_t *transferred);
} hal_spi_ops_t;

struct hal_spi {
    const hal_spi_ops_t *ops;
    void *ctx;
};

#endif
