#include "hal_net.h"

#include <stddef.h>

static sns_status_t template_net_connect(void *ctx, const char *host,
                                          uint16_t port, uint32_t timeout_ms)
{
    (void)ctx;
    (void)port;
    (void)timeout_ms;
    if (host == NULL) {
        return SNS_ERR_PARAM;
    }
    return SNS_ERR_UNSUPPORTED;
}

static sns_status_t template_net_send(void *ctx, const uint8_t *data,
                                       uint16_t length, uint16_t *sent)
{
    (void)ctx;
    if ((sent == NULL) || ((length > 0U) && (data == NULL))) {
        return SNS_ERR_PARAM;
    }
    *sent = 0U;
    return SNS_ERR_UNSUPPORTED;
}

static sns_status_t template_net_recv(void *ctx, uint8_t *data,
                                       uint16_t capacity, uint16_t *received)
{
    (void)ctx;
    if ((received == NULL) || ((capacity > 0U) && (data == NULL))) {
        return SNS_ERR_PARAM;
    }
    *received = 0U;
    return SNS_ERR_UNSUPPORTED;
}

static sns_status_t template_net_close(void *ctx)
{
    (void)ctx;
    return SNS_ERR_UNSUPPORTED;
}

static const hal_net_ops_t template_net_ops = {
    template_net_connect,
    template_net_send,
    template_net_recv,
    template_net_close
};

sns_status_t template_hal_net_bind(void *ctx, hal_net_t *hal)
{
    if (hal == NULL) {
        return SNS_ERR_PARAM;
    }
    hal->ops = &template_net_ops;
    hal->ctx = ctx;
    return SNS_OK;
}
