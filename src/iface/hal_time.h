#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

#include "util_status.h"

typedef struct hal_time hal_time_t;

typedef struct {
    sns_status_t (*now_ms)(void *ctx, uint32_t *now_ms);
} hal_time_ops_t;

struct hal_time {
    const hal_time_ops_t *ops;
    void *ctx;
};

#endif
