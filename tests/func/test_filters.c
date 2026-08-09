#include <stdint.h>
#include <stdio.h>

#include "func_filter.h"
#include "func_filter_ema.h"
#include "func_filter_limit.h"
#include "func_filter_ma.h"
#include "func_filter_median.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    int32_t operand;
    uint16_t process_calls;
    sns_status_t process_status;
} custom_filter_state_t;

typedef struct {
    int32_t operand;
    sns_status_t process_status;
} custom_filter_cfg_t;

static sns_status_t custom_filter_init(void *state, uint16_t state_size,
                                       const void *cfg)
{
    custom_filter_state_t *custom = state;
    const custom_filter_cfg_t *config = cfg;

    if ((custom == NULL) || (config == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (state_size < (uint16_t)sizeof(*custom)) {
        return SNS_ERR_NO_SPACE;
    }
    custom->operand = config->operand;
    custom->process_calls = 0U;
    custom->process_status = config->process_status;
    return SNS_OK;
}

static sns_status_t custom_filter_reset(void *state, const void *cfg)
{
    return custom_filter_init(state, (uint16_t)sizeof(custom_filter_state_t), cfg);
}

static sns_status_t custom_filter_add(void *state, func_filter_value_t input,
                                      func_filter_value_t *output)
{
    custom_filter_state_t *custom = state;

    if ((custom == NULL) || (output == NULL)) {
        return SNS_ERR_PARAM;
    }
    custom->process_calls++;
    if (custom->process_status != SNS_OK) {
        return custom->process_status;
    }
    *output = input + custom->operand;
    return SNS_OK;
}

static const func_filter_ops_t custom_add_ops = {
    custom_filter_init,
    custom_filter_reset,
    custom_filter_add
};

TEST(moving_average_uses_literal_rolling_window_values)
{
    func_filter_ma_cfg_t cfg = { 3U };
    func_filter_ma_state_t state;
    func_filter_instance_t instance = {
        &func_filter_ma_ops, &state, (uint16_t)sizeof(state), &cfg
    };
    func_filter_chain_t chain = { &instance, 1U };
    int32_t output = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_init(&chain));
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 10, &output));
    TEST_ASSERT_EQ_I32(10, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 20, &output));
    TEST_ASSERT_EQ_I32(15, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 60, &output));
    TEST_ASSERT_EQ_I32(30, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 40, &output));
    TEST_ASSERT_EQ_I32(40, output);
}

TEST(ema_uses_q15_half_weight_and_rounds_literal_values)
{
    func_filter_ema_cfg_t cfg = { UINT16_C(16384) };
    func_filter_ema_state_t state;
    func_filter_instance_t instance = {
        &func_filter_ema_ops, &state, (uint16_t)sizeof(state), &cfg
    };
    func_filter_chain_t chain = { &instance, 1U };
    int32_t output = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_init(&chain));
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 100, &output));
    TEST_ASSERT_EQ_I32(100, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 200, &output));
    TEST_ASSERT_EQ_I32(150, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 151, &output));
    TEST_ASSERT_EQ_I32(151, output);
}

TEST(median_rejects_outlier_with_hand_sorted_expectations)
{
    func_filter_median_cfg_t cfg = { 3U };
    func_filter_median_state_t state;
    func_filter_instance_t instance = {
        &func_filter_median_ops, &state, (uint16_t)sizeof(state), &cfg
    };
    func_filter_chain_t chain = { &instance, 1U };
    int32_t output = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_init(&chain));
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 9, &output));
    TEST_ASSERT_EQ_I32(9, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 1, &output));
    TEST_ASSERT_EQ_I32(5, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 5, &output));
    TEST_ASSERT_EQ_I32(5, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 100, &output));
    TEST_ASSERT_EQ_I32(5, output);
}

TEST(limit_filter_applies_range_and_change_boundaries)
{
    func_filter_limit_cfg_t cfg = { 0, 100, 10 };
    func_filter_limit_state_t state;
    func_filter_instance_t instance = {
        &func_filter_limit_ops, &state, (uint16_t)sizeof(state), &cfg
    };
    func_filter_chain_t chain = { &instance, 1U };
    int32_t output = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_init(&chain));
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, -5, &output));
    TEST_ASSERT_EQ_I32(0, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 50, &output));
    TEST_ASSERT_EQ_I32(10, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, -10, &output));
    TEST_ASSERT_EQ_I32(0, output);
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 200, &output));
    TEST_ASSERT_EQ_I32(10, output);
}

TEST(custom_ops_compose_and_failure_stops_the_chain)
{
    custom_filter_cfg_t first_cfg = { 10, SNS_OK };
    custom_filter_cfg_t failing_cfg = { 20, SNS_ERR_IO };
    custom_filter_cfg_t never_cfg = { 30, SNS_OK };
    custom_filter_state_t first_state;
    custom_filter_state_t failing_state;
    custom_filter_state_t never_state;
    func_filter_instance_t items[3] = {
        { &custom_add_ops, &first_state, (uint16_t)sizeof(first_state), &first_cfg },
        { &custom_add_ops, &failing_state, (uint16_t)sizeof(failing_state), &failing_cfg },
        { &custom_add_ops, &never_state, (uint16_t)sizeof(never_state), &never_cfg }
    };
    func_filter_chain_t chain = { items, 3U };
    int32_t output = INT32_C(777777);

    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_init(&chain));
    TEST_ASSERT_EQ_I32(SNS_ERR_IO, func_filter_chain_process(&chain, 5, &output));
    TEST_ASSERT_EQ_I32(777777, output);
    TEST_ASSERT_EQ_U16(1U, first_state.process_calls);
    TEST_ASSERT_EQ_U16(1U, failing_state.process_calls);
    TEST_ASSERT_EQ_U16(0U, never_state.process_calls);

    failing_state.process_status = SNS_OK;
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, 5, &output));
    TEST_ASSERT_EQ_I32(65, output);
}

TEST(zero_filter_chain_is_a_real_pass_through)
{
    func_filter_chain_t chain = { NULL, 0U };
    int32_t output = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_init(&chain));
    TEST_ASSERT_EQ_I32(SNS_OK, func_filter_chain_process(&chain, -1234, &output));
    TEST_ASSERT_EQ_I32(-1234, output);
}

int main(void)
{
    moving_average_uses_literal_rolling_window_values();
    ema_uses_q15_half_weight_and_rounds_literal_values();
    median_rejects_outlier_with_hand_sorted_expectations();
    limit_filter_applies_range_and_change_boundaries();
    custom_ops_compose_and_failure_stops_the_chain();
    zero_filter_chain_is_a_real_pass_through();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
