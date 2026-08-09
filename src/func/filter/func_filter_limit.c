#include "func_filter_limit.h"

#include <stddef.h>

static sns_status_t limit_configure(void *state, uint16_t state_size,
                                    const void *cfg)
{
    func_filter_limit_state_t *limit;
    const func_filter_limit_cfg_t *config;

    if ((state == NULL) || (cfg == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (state_size < (uint16_t)sizeof(func_filter_limit_state_t)) {
        return SNS_ERR_NO_SPACE;
    }
    config = (const func_filter_limit_cfg_t *)cfg;
    if ((config->min_value > config->max_value) ||
        (config->max_change < 0)) {
        return SNS_ERR_PARAM;
    }
    limit = (func_filter_limit_state_t *)state;
    limit->min_value = config->min_value;
    limit->max_value = config->max_value;
    limit->max_change = config->max_change;
    limit->previous = 0;
    limit->initialized = 0U;
    return SNS_OK;
}

static sns_status_t limit_init(void *state, uint16_t state_size,
                               const void *cfg)
{
    return limit_configure(state, state_size, cfg);
}

static sns_status_t limit_reset(void *state, const void *cfg)
{
    return limit_configure(state,
                           (uint16_t)sizeof(func_filter_limit_state_t), cfg);
}

static sns_status_t limit_process(void *state, func_filter_value_t input,
                                  func_filter_value_t *output)
{
    func_filter_limit_state_t *limit;
    int32_t limited;
    int64_t upper;
    int64_t lower;

    if ((state == NULL) || (output == NULL)) {
        return SNS_ERR_PARAM;
    }
    limit = (func_filter_limit_state_t *)state;
    if ((limit->min_value > limit->max_value) || (limit->max_change < 0)) {
        return SNS_ERR_STATE;
    }

    limited = input;
    if (limited < limit->min_value) {
        limited = limit->min_value;
    } else if (limited > limit->max_value) {
        limited = limit->max_value;
    }
    if (limit->initialized != 0U) {
        upper = (int64_t)limit->previous + (int64_t)limit->max_change;
        lower = (int64_t)limit->previous - (int64_t)limit->max_change;
        if ((int64_t)limited > upper) {
            limited = (int32_t)upper;
        } else if ((int64_t)limited < lower) {
            limited = (int32_t)lower;
        }
    }
    limit->previous = limited;
    limit->initialized = 1U;
    *output = limited;
    return SNS_OK;
}

const func_filter_ops_t func_filter_limit_ops = {
    limit_init,
    limit_reset,
    limit_process
};
