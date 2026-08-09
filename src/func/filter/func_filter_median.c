#include "func_filter_median.h"

#include <stddef.h>

#include "util_math.h"

static sns_status_t median_configure(void *state, uint16_t state_size,
                                     const void *cfg)
{
    func_filter_median_state_t *median;
    const func_filter_median_cfg_t *config;
    uint16_t index;

    if ((state == NULL) || (cfg == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (state_size < (uint16_t)sizeof(func_filter_median_state_t)) {
        return SNS_ERR_NO_SPACE;
    }
    config = (const func_filter_median_cfg_t *)cfg;
    if ((config->window_size == 0U) ||
        (config->window_size > FUNC_CFG_FILTER_MEDIAN_MAX_WINDOW)) {
        return SNS_ERR_PARAM;
    }
    median = (func_filter_median_state_t *)state;
    for (index = 0U; index < FUNC_CFG_FILTER_MEDIAN_MAX_WINDOW; ++index) {
        median->samples[index] = 0;
    }
    median->count = 0U;
    median->next_index = 0U;
    median->window_size = config->window_size;
    return SNS_OK;
}

static sns_status_t median_init(void *state, uint16_t state_size,
                                const void *cfg)
{
    return median_configure(state, state_size, cfg);
}

static sns_status_t median_reset(void *state, const void *cfg)
{
    return median_configure(state,
                            (uint16_t)sizeof(func_filter_median_state_t), cfg);
}

static sns_status_t median_process(void *state, func_filter_value_t input,
                                   func_filter_value_t *output)
{
    func_filter_median_state_t *median;
    int32_t sorted[FUNC_CFG_FILTER_MEDIAN_MAX_WINDOW];
    int32_t key;
    uint16_t index;
    uint16_t position;
    int64_t middle;

    if ((state == NULL) || (output == NULL)) {
        return SNS_ERR_PARAM;
    }
    median = (func_filter_median_state_t *)state;
    if ((median->window_size == 0U) ||
        (median->window_size > FUNC_CFG_FILTER_MEDIAN_MAX_WINDOW) ||
        (median->count > median->window_size) ||
        (median->next_index >= median->window_size)) {
        return SNS_ERR_STATE;
    }

    median->samples[median->next_index] = input;
    median->next_index =
        (uint16_t)((median->next_index + 1U) % median->window_size);
    if (median->count < median->window_size) {
        ++median->count;
    }
    for (index = 0U; index < median->count; ++index) {
        sorted[index] = median->samples[index];
    }
    for (index = 1U; index < median->count; ++index) {
        key = sorted[index];
        position = index;
        while ((position > 0U) && (sorted[position - 1U] > key)) {
            sorted[position] = sorted[position - 1U];
            --position;
        }
        sorted[position] = key;
    }

    if ((median->count & 1U) != 0U) {
        *output = sorted[median->count / 2U];
    } else {
        middle = (int64_t)sorted[(median->count / 2U) - 1U] +
                 (int64_t)sorted[median->count / 2U];
        if (util_div_round_nearest_i64(middle, 2, &middle) != SNS_OK) {
            return SNS_ERR_STATE;
        }
        *output = (func_filter_value_t)middle;
    }
    return SNS_OK;
}

const func_filter_ops_t func_filter_median_ops = {
    median_init,
    median_reset,
    median_process
};
