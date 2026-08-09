#include "hal_time.h"

#include <stddef.h>

static sns_status_t template_time_now_ms(void *ctx, uint32_t *now_ms)
{
    (void)ctx;
    if (now_ms == NULL) {
        return SNS_ERR_PARAM;
    }
    return SNS_ERR_UNSUPPORTED;
}

static const hal_time_ops_t template_time_ops = {
    template_time_now_ms
};

sns_status_t template_hal_time_bind(void *ctx, hal_time_t *hal)
{
    if (hal == NULL) {
        return SNS_ERR_PARAM;
    }
    hal->ops = &template_time_ops;
    hal->ctx = ctx;
    return SNS_OK;
}
