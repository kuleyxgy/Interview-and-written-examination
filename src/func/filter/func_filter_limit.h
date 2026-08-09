#ifndef FUNC_FILTER_LIMIT_H
#define FUNC_FILTER_LIMIT_H

#include <stdint.h>

#include "func_filter.h"

typedef struct {
    int32_t min_value;
    int32_t max_value;
    int32_t max_change;
} func_filter_limit_cfg_t;

typedef struct {
    int32_t min_value;
    int32_t max_value;
    int32_t max_change;
    int32_t previous;
    uint8_t initialized;
} func_filter_limit_state_t;

extern const func_filter_ops_t func_filter_limit_ops;

#endif
