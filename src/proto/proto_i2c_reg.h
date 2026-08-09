#ifndef PROTO_I2C_REG_H
#define PROTO_I2C_REG_H

#include <stdint.h>

#include "hal_i2c.h"
#include "util_status.h"

typedef struct {
    hal_i2c_t *bus;
    uint16_t address;
    uint8_t address_bits;
    uint32_t timeout_ms;
} proto_i2c_device_t;

sns_status_t proto_i2c_device_init(proto_i2c_device_t *device,
                                    hal_i2c_t *bus,
                                    uint16_t address,
                                    uint8_t address_bits,
                                    uint32_t timeout_ms);
sns_status_t proto_i2c_reg_read(proto_i2c_device_t *device,
                                const uint8_t *reg,
                                uint8_t reg_len,
                                uint8_t *data,
                                uint16_t data_capacity);
sns_status_t proto_i2c_reg_write(proto_i2c_device_t *device,
                                 const uint8_t *reg,
                                 uint8_t reg_len,
                                 const uint8_t *data,
                                 uint16_t data_len);

#endif
