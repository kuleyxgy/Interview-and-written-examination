#ifndef FUNC_FILTER_H
#define FUNC_FILTER_H

#include <stdint.h>

#include "util_status.h"

typedef int32_t func_filter_value_t;

typedef struct {
    sns_status_t (*init)(void *state, uint16_t state_size, const void *cfg);
    sns_status_t (*reset)(void *state, const void *cfg);
    sns_status_t (*process)(void *state,
                            func_filter_value_t input,
                            func_filter_value_t *output);
} func_filter_ops_t;

typedef struct {
    const func_filter_ops_t *ops;
    void *state;
    uint16_t state_size;
    const void *cfg;
} func_filter_instance_t;

typedef struct {
    func_filter_instance_t *items;
    uint8_t count;
} func_filter_chain_t;

sns_status_t func_filter_chain_init(func_filter_chain_t *chain);
sns_status_t func_filter_chain_reset(func_filter_chain_t *chain);
sns_status_t func_filter_chain_process(func_filter_chain_t *chain,
                                       func_filter_value_t input,
                                       func_filter_value_t *output);

#endif
