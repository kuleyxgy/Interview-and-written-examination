#include "host_time.h"

#include <stddef.h>

static sns_status_t host_time_now_ms(void *ctx, uint32_t *now_ms)
{
    const host_time_t *clock = (const host_time_t *)ctx;
    uint32_t value;

    if ((clock == NULL) || (now_ms == NULL)) {
        return SNS_ERR_PARAM;
    }

    value = clock->now_ms;
    *now_ms = value;
    return SNS_OK;
}

static const hal_time_ops_t host_time_ops = {
    host_time_now_ms
};

sns_status_t host_time_init(host_time_t *clock, uint32_t initial_ms)
{
    if (clock == NULL) {
        return SNS_ERR_PARAM;
    }

    clock->now_ms = initial_ms;
    return SNS_OK;
}

sns_status_t host_time_bind(host_time_t *clock, hal_time_t *hal)
{
    if ((clock == NULL) || (hal == NULL)) {
        return SNS_ERR_PARAM;
    }

    hal->ops = &host_time_ops;
    hal->ctx = clock;
    return SNS_OK;
}

sns_status_t host_time_set(host_time_t *clock, uint32_t now_ms)
{
    if (clock == NULL) {
        return SNS_ERR_PARAM;
    }

    clock->now_ms = now_ms;
    return SNS_OK;
}

sns_status_t host_time_advance(host_time_t *clock, uint32_t elapsed_ms)
{
    if (clock == NULL) {
        return SNS_ERR_PARAM;
    }

    clock->now_ms += elapsed_ms;
    return SNS_OK;
}
