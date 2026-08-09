#include "func_filter_ma.h"

#include <stddef.h>

#include "util_math.h"

static sns_status_t ma_configure(void *state, uint16_t state_size,
                                 const void *cfg)
{
    func_filter_ma_state_t *ma;
    const func_filter_ma_cfg_t *config;
    uint16_t index;

    if ((state == NULL) || (cfg == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (state_size < (uint16_t)sizeof(func_filter_ma_state_t)) {
        return SNS_ERR_NO_SPACE;
    }
    config = (const func_filter_ma_cfg_t *)cfg;
    if ((config->window_size == 0U) ||
        (config->window_size > FUNC_CFG_FILTER_MA_MAX_WINDOW)) {
        return SNS_ERR_PARAM;
    }
    ma = (func_filter_ma_state_t *)state;
    for (index = 0U; index < FUNC_CFG_FILTER_MA_MAX_WINDOW; ++index) {
        ma->samples[index] = 0;
    }
    ma->sum = 0;
    ma->count = 0U;
    ma->next_index = 0U;
    ma->window_size = config->window_size;
    return SNS_OK;
}

static sns_status_t ma_init(void *state, uint16_t state_size, const void *cfg)
{
    return ma_configure(state, state_size, cfg);
}

static sns_status_t ma_reset(void *state, const void *cfg)
{
    return ma_configure(state, (uint16_t)sizeof(func_filter_ma_state_t), cfg);
}

static sns_status_t ma_process(void *state, func_filter_value_t input,
                               func_filter_value_t *output)
{
    func_filter_ma_state_t *ma;
    int64_t average;
    sns_status_t status;

    if ((state == NULL) || (output == NULL)) {
        return SNS_ERR_PARAM;
    }
    ma = (func_filter_ma_state_t *)state;
    if ((ma->window_size == 0U) ||
        (ma->window_size > FUNC_CFG_FILTER_MA_MAX_WINDOW) ||
        (ma->count > ma->window_size) || (ma->next_index >= ma->window_size)) {
        return SNS_ERR_STATE;
    }

    if (ma->count == ma->window_size) {
        ma->sum -= (int64_t)ma->samples[ma->next_index];
    } else {
        ++ma->count;
    }
    ma->samples[ma->next_index] = input;
    ma->sum += (int64_t)input;
    ma->next_index = (uint16_t)((ma->next_index + 1U) % ma->window_size);

    status = util_div_round_nearest_i64(ma->sum, (int64_t)ma->count, &average);
    if (status != SNS_OK) {
        return status;
    }
    *output = (func_filter_value_t)average;
    return SNS_OK;
}

const func_filter_ops_t func_filter_ma_ops = {
    ma_init,
    ma_reset,
    ma_process
};
