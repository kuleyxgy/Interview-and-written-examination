#include <stdint.h>
#include <stdio.h>

#include "func_app_runtime.h"
#include "func_sensor.h"
#include "func_temp.h"
#include "hal_time.h"
#include "host_time.h"
#include "proto_clock.h"
#include "proto_temp.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    int32_t value;
    uint16_t reads;
} e2e_source_t;

static sns_status_t e2e_source_init(void *context)
{
    e2e_source_t *source = context;
    if (source == NULL) {
        return SNS_ERR_PARAM;
    }
    source->reads = 0U;
    return SNS_OK;
}

static sns_status_t e2e_source_read(void *context, int32_t *value_mdeg_c)
{
    e2e_source_t *source = context;
    if ((source == NULL) || (value_mdeg_c == NULL)) {
        return SNS_ERR_PARAM;
    }
    source->reads++;
    *value_mdeg_c = source->value;
    return SNS_OK;
}

static const proto_temp_ops_t e2e_source_ops = {
    e2e_source_init,
    e2e_source_read
};

TEST(runtime_core_clock_temp_pipeline_runs_without_optional_apps)
{
    host_time_t host_clock;
    hal_time_t hal_time;
    proto_clock_t clock;
    e2e_source_t source = { 12345, 0U };
    proto_temp_device_t device = { &e2e_source_ops, &source };
    func_temp_cfg_t cfg;
    func_temp_t temp;
    func_sensor_registration_t registration = {
        5U, "e2e-temp", &func_temp_driver_ops, &temp
    };
    func_sensor_core_t core;
    func_app_runtime_t runtime;
    func_sensor_event_t latest;
    uint32_t now_ms = 0U;

    cfg.source = &device;
    cfg.sample_period_ms = 10U;
    cfg.calibration_gain_ppm = 0;
    cfg.calibration_offset_mdeg_c = 0;
    cfg.filters.items = NULL;
    cfg.filters.count = 0U;
    cfg.publish_change_mdeg_c = 0;
    cfg.force_publish_period_ms = 0U;
    cfg.error_threshold = 2U;

    TEST_ASSERT_EQ_I32(SNS_OK, host_time_init(&host_clock, 42U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_time_bind(&host_clock, &hal_time));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_clock_init(&clock, &hal_time));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_core_init(&core));
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_configure(&temp, &cfg));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_register(&core, &registration));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_runtime_init(&runtime, &core, &clock,
                                                      NULL, NULL, NULL, NULL, 0U));

    TEST_ASSERT_EQ_I32(SNS_OK, func_app_runtime_poll_once(&runtime, &now_ms));
    TEST_ASSERT_EQ_U32(42U, now_ms);
    TEST_ASSERT_EQ_U16(1U, source.reads);
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_get_latest(&core, 5U, &latest));
    TEST_ASSERT_EQ_U16(5U, latest.sensor_id);
    TEST_ASSERT_EQ_I32(12345, latest.value);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_VALID, latest.quality);
    TEST_ASSERT_EQ_I32(SNS_OK, latest.status);
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_reset(&core));
}

int main(void)
{
    runtime_core_clock_temp_pipeline_runs_without_optional_apps();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }
    return 0;
}
