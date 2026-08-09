#include "proto_i2c_reg.h"

#include <stddef.h>
#include <string.h>

#include "proto_cfg.h"

static sns_status_t proto_i2c_device_validate(const proto_i2c_device_t *device)
{
    if ((device == NULL) || (device->bus == NULL) ||
        (device->bus->ops == NULL) ||
        (device->bus->ops->transfer == NULL) ||
        (device->timeout_ms == 0U)) {
        return SNS_ERR_PARAM;
    }
    if (((device->address_bits == 7U) && (device->address > UINT16_C(0x007F))) ||
        ((device->address_bits == 10U) && (device->address > UINT16_C(0x03FF)))) {
        return SNS_ERR_PARAM;
    }
    if ((device->address_bits != 7U) && (device->address_bits != 10U)) {
        return SNS_ERR_UNSUPPORTED;
    }

    return SNS_OK;
}

sns_status_t proto_i2c_device_init(proto_i2c_device_t *device,
                                    hal_i2c_t *bus,
                                    uint16_t address,
                                    uint8_t address_bits,
                                    uint32_t timeout_ms)
{
    proto_i2c_device_t candidate;
    sns_status_t status;

    if (device == NULL) {
        return SNS_ERR_PARAM;
    }

    candidate.bus = bus;
    candidate.address = address;
    candidate.address_bits = address_bits;
    candidate.timeout_ms = timeout_ms;
    status = proto_i2c_device_validate(&candidate);
    if (status != SNS_OK) {
        return status;
    }

    *device = candidate;
    return SNS_OK;
}

sns_status_t proto_i2c_reg_read(proto_i2c_device_t *device,
                                const uint8_t *reg,
                                uint8_t reg_len,
                                uint8_t *data,
                                uint16_t data_capacity)
{
    uint8_t staging[PROTO_CFG_I2C_MAX_DATA];
    uint16_t transferred = 0U;
    sns_status_t status;

    status = proto_i2c_device_validate(device);
    if (status != SNS_OK) {
        return status;
    }
    if ((reg == NULL) || (reg_len == 0U) ||
        (data == NULL) || (data_capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    if ((reg_len > PROTO_CFG_I2C_MAX_REGISTER_BYTES) ||
        (data_capacity > PROTO_CFG_I2C_MAX_DATA)) {
        return SNS_ERR_NO_SPACE;
    }

    status = device->bus->ops->transfer(device->bus->ctx, device->address,
                                        reg, reg_len, staging, data_capacity,
                                        device->timeout_ms, &transferred);
    if (status != SNS_OK) {
        return status;
    }
    if (transferred != data_capacity) {
        return SNS_ERR_IO;
    }

    (void)memcpy(data, staging, data_capacity);
    return SNS_OK;
}

sns_status_t proto_i2c_reg_write(proto_i2c_device_t *device,
                                 const uint8_t *reg,
                                 uint8_t reg_len,
                                 const uint8_t *data,
                                 uint16_t data_len)
{
    uint8_t staging[PROTO_CFG_I2C_MAX_REGISTER_BYTES + PROTO_CFG_I2C_MAX_DATA];
    uint16_t transferred = 0U;
    uint16_t total_len;
    sns_status_t status;

    status = proto_i2c_device_validate(device);
    if (status != SNS_OK) {
        return status;
    }
    if ((reg == NULL) || (reg_len == 0U) ||
        ((data_len > 0U) && (data == NULL))) {
        return SNS_ERR_PARAM;
    }
    if ((reg_len > PROTO_CFG_I2C_MAX_REGISTER_BYTES) ||
        (data_len > PROTO_CFG_I2C_MAX_DATA)) {
        return SNS_ERR_NO_SPACE;
    }

    total_len = (uint16_t)((uint16_t)reg_len + data_len);
    (void)memcpy(staging, reg, reg_len);
    if (data_len > 0U) {
        (void)memcpy(&staging[reg_len], data, data_len);
    }

    status = device->bus->ops->transfer(device->bus->ctx, device->address,
                                        staging, total_len, NULL, 0U,
                                        device->timeout_ms, &transferred);
    if (status != SNS_OK) {
        return status;
    }
    if (transferred != 0U) {
        return SNS_ERR_INVALID_DATA;
    }

    return SNS_OK;
}
