#include "func_filter_ema.h"

#include <stddef.h>

#include "util_math.h"

static sns_status_t ema_configure(void *state, uint16_t state_size,
                                  const void *cfg)
{
    func_filter_ema_state_t *ema;
    const func_filter_ema_cfg_t *config;

    if ((state == NULL) || (cfg == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (state_size < (uint16_t)sizeof(func_filter_ema_state_t)) {
        return SNS_ERR_NO_SPACE;
    }
    config = (const func_filter_ema_cfg_t *)cfg;
    if ((config->alpha_q15 == 0U) ||
        (config->alpha_q15 > FUNC_FILTER_EMA_Q15_ONE)) {
        return SNS_ERR_PARAM;
    }
    ema = (func_filter_ema_state_t *)state;
    ema->value = 0;
    ema->alpha_q15 = config->alpha_q15;
    ema->initialized = 0U;
    return SNS_OK;
}

static sns_status_t ema_init(void *state, uint16_t state_size, const void *cfg)
{
    return ema_configure(state, state_size, cfg);
}

static sns_status_t ema_reset(void *state, const void *cfg)
{
    return ema_configure(state, (uint16_t)sizeof(func_filter_ema_state_t), cfg);
}

static sns_status_t ema_process(void *state, func_filter_value_t input,
                                func_filter_value_t *output)
{
    func_filter_ema_state_t *ema;
    int64_t adjustment;
    int64_t updated;
    sns_status_t status;

    if ((state == NULL) || (output == NULL)) {
        return SNS_ERR_PARAM;
    }
    ema = (func_filter_ema_state_t *)state;
    if ((ema->alpha_q15 == 0U) ||
        (ema->alpha_q15 > FUNC_FILTER_EMA_Q15_ONE)) {
        return SNS_ERR_STATE;
    }
    if (ema->initialized == 0U) {
        ema->value = input;
        ema->initialized = 1U;
        *output = input;
        return SNS_OK;
    }

    status = util_div_round_nearest_i64(
        (int64_t)ema->alpha_q15 * ((int64_t)input - (int64_t)ema->value),
        (int64_t)FUNC_FILTER_EMA_Q15_ONE,
        &adjustment);
    if (status != SNS_OK) {
        return status;
    }
    updated = (int64_t)ema->value + adjustment;
    ema->value = util_sat_i64_to_i32(updated);
    *output = ema->value;
    return SNS_OK;
}

const func_filter_ops_t func_filter_ema_ops = {
    ema_init,
    ema_reset,
    ema_process
};
