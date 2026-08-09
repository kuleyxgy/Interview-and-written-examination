#ifndef FUNC_FILTER_MA_H
#define FUNC_FILTER_MA_H

#include <stdint.h>

#include "func_cfg.h"
#include "func_filter.h"

typedef struct {
    uint16_t window_size;
} func_filter_ma_cfg_t;

typedef struct {
    int32_t samples[FUNC_CFG_FILTER_MA_MAX_WINDOW];
    int64_t sum;
    uint16_t count;
    uint16_t next_index;
    uint16_t window_size;
} func_filter_ma_state_t;

extern const func_filter_ops_t func_filter_ma_ops;

#endif
