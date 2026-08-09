#ifndef HOST_I2C_H
#define HOST_I2C_H

#include <stdint.h>

#include "hal_cfg.h"
#include "hal_i2c.h"
#include "util_status.h"

typedef sns_status_t (*host_i2c_device_transfer_fn)(void *ctx,
                                                     const uint8_t *write_data,
                                                     uint16_t write_length,
                                                     uint8_t *read_data,
                                                     uint16_t read_capacity,
                                                     uint32_t timeout_ms,
                                                     uint16_t *transferred);

typedef struct {
    uint16_t address;
    host_i2c_device_transfer_fn transfer;
    void *ctx;
    uint8_t in_use;
} host_i2c_device_t;

typedef struct {
    host_i2c_device_t devices[HAL_CFG_HOST_I2C_MAX_DEVICES];
    uint8_t rx_staging[HAL_CFG_HOST_I2C_MAX_TRANSFER];
} host_i2c_bus_t;

sns_status_t host_i2c_bus_init(host_i2c_bus_t *bus);
sns_status_t host_i2c_bus_bind(host_i2c_bus_t *bus, hal_i2c_t *hal);
sns_status_t host_i2c_bus_register(host_i2c_bus_t *bus,
                                   uint16_t address,
                                   host_i2c_device_transfer_fn transfer,
                                   void *ctx);

#endif
