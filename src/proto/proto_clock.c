#include "proto_clock.h"

#include <stddef.h>

#include "hal_time.h"

sns_status_t proto_clock_init(proto_clock_t *clock, hal_time_t *time)
{
    if ((clock == NULL) || (time == NULL) ||
        (time->ops == NULL) || (time->ops->now_ms == NULL)) {
        return SNS_ERR_PARAM;
    }

    clock->time = time;
    return SNS_OK;
}

sns_status_t proto_clock_now_ms(proto_clock_t *clock, uint32_t *now_ms)
{
    uint32_t value;
    sns_status_t status;

    if ((clock == NULL) || (clock->time == NULL) ||
        (clock->time->ops == NULL) ||
        (clock->time->ops->now_ms == NULL) || (now_ms == NULL)) {
        return SNS_ERR_PARAM;
    }

    status = clock->time->ops->now_ms(clock->time->ctx, &value);
    if (status != SNS_OK) {
        return status;
    }

    *now_ms = value;
    return SNS_OK;
}
