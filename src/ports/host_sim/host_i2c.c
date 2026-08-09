#include "host_i2c.h"

#include <stddef.h>
#include <string.h>

static host_i2c_device_t *host_i2c_find(host_i2c_bus_t *bus, uint16_t address)
{
    uint16_t index;

    for (index = 0U; index < HAL_CFG_HOST_I2C_MAX_DEVICES; ++index) {
        if ((bus->devices[index].in_use != 0U) &&
            (bus->devices[index].address == address)) {
            return &bus->devices[index];
        }
    }

    return NULL;
}

static sns_status_t host_i2c_transfer(void *ctx,
                                      uint16_t address,
                                      const uint8_t *tx,
                                      uint16_t tx_len,
                                      uint8_t *rx,
                                      uint16_t rx_capacity,
                                      uint32_t timeout_ms,
                                      uint16_t *transferred)
{
    host_i2c_bus_t *bus = (host_i2c_bus_t *)ctx;
    host_i2c_device_t *device;
    uint16_t completed = 0U;
    sns_status_t status;

    if ((bus == NULL) || (transferred == NULL) ||
        ((tx_len > 0U) && (tx == NULL)) ||
        ((rx_capacity > 0U) && (rx == NULL))) {
        return SNS_ERR_PARAM;
    }
    if (rx_capacity > HAL_CFG_HOST_I2C_MAX_TRANSFER) {
        return SNS_ERR_NO_SPACE;
    }

    device = host_i2c_find(bus, address);
    if (device == NULL) {
        return SNS_ERR_NOT_FOUND;
    }

    status = device->transfer(device->ctx, tx, tx_len,
                              (rx_capacity > 0U) ? bus->rx_staging : NULL,
                              rx_capacity, timeout_ms, &completed);
    if (status != SNS_OK) {
        return status;
    }
    if (completed > rx_capacity) {
        return SNS_ERR_INVALID_DATA;
    }

    if (completed > 0U) {
        (void)memcpy(rx, bus->rx_staging, completed);
    }
    *transferred = completed;
    return SNS_OK;
}

static const hal_i2c_ops_t host_i2c_ops = {
    host_i2c_transfer
};

sns_status_t host_i2c_bus_init(host_i2c_bus_t *bus)
{
    if (bus == NULL) {
        return SNS_ERR_PARAM;
    }

    (void)memset(bus, 0, sizeof(*bus));
    return SNS_OK;
}

sns_status_t host_i2c_bus_bind(host_i2c_bus_t *bus, hal_i2c_t *hal)
{
    if ((bus == NULL) || (hal == NULL)) {
        return SNS_ERR_PARAM;
    }

    hal->ops = &host_i2c_ops;
    hal->ctx = bus;
    return SNS_OK;
}

sns_status_t host_i2c_bus_register(host_i2c_bus_t *bus,
                                   uint16_t address,
                                   host_i2c_device_transfer_fn transfer,
                                   void *ctx)
{
    uint16_t index;

    if ((bus == NULL) || (transfer == NULL) || (address > UINT16_C(0x03FF))) {
        return SNS_ERR_PARAM;
    }
    if (host_i2c_find(bus, address) != NULL) {
        return SNS_ERR_STATE;
    }

    for (index = 0U; index < HAL_CFG_HOST_I2C_MAX_DEVICES; ++index) {
        if (bus->devices[index].in_use == 0U) {
            bus->devices[index].address = address;
            bus->devices[index].transfer = transfer;
            bus->devices[index].ctx = ctx;
            bus->devices[index].in_use = 1U;
            return SNS_OK;
        }
    }

    return SNS_ERR_NO_SPACE;
}
