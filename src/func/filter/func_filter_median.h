#ifndef FUNC_FILTER_MEDIAN_H
#define FUNC_FILTER_MEDIAN_H

#include <stdint.h>

#include "func_cfg.h"
#include "func_filter.h"

typedef struct {
    uint16_t window_size;
} func_filter_median_cfg_t;

typedef struct {
    int32_t samples[FUNC_CFG_FILTER_MEDIAN_MAX_WINDOW];
    uint16_t count;
    uint16_t next_index;
    uint16_t window_size;
} func_filter_median_state_t;

extern const func_filter_ops_t func_filter_median_ops;

#endif
