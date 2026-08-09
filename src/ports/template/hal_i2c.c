#include "hal_i2c.h"

#include <stddef.h>

static sns_status_t template_i2c_transfer(void *ctx, uint16_t address,
                                           const uint8_t *tx, uint16_t tx_len,
                                           uint8_t *rx, uint16_t rx_capacity,
                                           uint32_t timeout_ms,
                                           uint16_t *transferred)
{
    (void)ctx;
    (void)address;
    (void)tx;
    (void)tx_len;
    (void)rx;
    (void)rx_capacity;
    (void)timeout_ms;
    if (transferred == NULL) {
        return SNS_ERR_PARAM;
    }
    *transferred = 0U;
    return SNS_ERR_UNSUPPORTED;
}

static const hal_i2c_ops_t template_i2c_ops = {
    template_i2c_transfer
};

sns_status_t template_hal_i2c_bind(void *ctx, hal_i2c_t *hal)
{
    if (hal == NULL) {
        return SNS_ERR_PARAM;
    }
    hal->ops = &template_i2c_ops;
    hal->ctx = ctx;
    return SNS_OK;
}
