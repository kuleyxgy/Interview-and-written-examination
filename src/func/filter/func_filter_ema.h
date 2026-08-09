#ifndef FUNC_FILTER_EMA_H
#define FUNC_FILTER_EMA_H

#include <stdint.h>

#include "func_filter.h"

#define FUNC_FILTER_EMA_Q15_ONE 32768U

typedef struct {
    uint16_t alpha_q15;
} func_filter_ema_cfg_t;

typedef struct {
    int32_t value;
    uint16_t alpha_q15;
    uint8_t initialized;
} func_filter_ema_state_t;

extern const func_filter_ops_t func_filter_ema_ops;

#endif
