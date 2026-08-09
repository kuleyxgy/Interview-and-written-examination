#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "func_event_queue.h"
#include "func_sensor.h"
#include "func_temp.h"
#include "proto_temp.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    int32_t values[4];
    sns_status_t statuses[4];
    uint16_t count;
    uint16_t index;
    uint16_t init_calls;
} temp_source_fixture_t;

static sns_status_t temp_source_init(void *context)
{
    temp_source_fixture_t *fixture = context;

    if (fixture == NULL) {
        return SNS_ERR_PARAM;
    }
    fixture->index = 0U;
    fixture->init_calls++;
    return SNS_OK;
}

static sns_status_t temp_source_read(void *context, int32_t *value_mdeg_c)
{
    temp_source_fixture_t *fixture = context;
    uint16_t index;

    if ((fixture == NULL) || (value_mdeg_c == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (fixture->index >= fixture->count) {
        return SNS_ERR_NOT_READY;
    }
    index = fixture->index;
    fixture->index++;
    if (fixture->statuses[index] != SNS_OK) {
        return fixture->statuses[index];
    }
    *value_mdeg_c = fixture->values[index];
    return SNS_OK;
}

static const proto_temp_ops_t temp_source_ops = {
    temp_source_init,
    temp_source_read
};

static func_temp_cfg_t make_temp_cfg(proto_temp_device_t *source,
                                     uint32_t sample_period_ms,
                                     uint16_t error_threshold)
{
    func_temp_cfg_t cfg;

    cfg.source = source;
    cfg.sample_period_ms = sample_period_ms;
    cfg.calibration_gain_ppm = 0;
    cfg.calibration_offset_mdeg_c = 0;
    cfg.filters.items = NULL;
    cfg.filters.count = 0U;
    cfg.publish_change_mdeg_c = 0;
    cfg.force_publish_period_ms = 0U;
    cfg.error_threshold = error_threshold;
    return cfg;
}

TEST(two_sensor_instances_and_three_subscriber_queues_remain_isolated)
{
    temp_source_fixture_t first_source = {
        { 10000, 11000, 0, 0 }, { SNS_OK, SNS_OK, SNS_OK, SNS_OK }, 2U, 0U, 0U
    };
    temp_source_fixture_t second_source = {
        { 20000, 21000, 0, 0 }, { SNS_OK, SNS_OK, SNS_OK, SNS_OK }, 2U, 0U, 0U
    };
    proto_temp_device_t first_device = { &temp_source_ops, &first_source };
    proto_temp_device_t second_device = { &temp_source_ops, &second_source };
    func_temp_cfg_t first_cfg = make_temp_cfg(&first_device, 10U, 2U);
    func_temp_cfg_t second_cfg = make_temp_cfg(&second_device, 10U, 2U);
    func_temp_t first_temp;
    func_temp_t second_temp;
    func_sensor_registration_t first_registration = {
        1U, "first", &func_temp_driver_ops, &first_temp
    };
    func_sensor_registration_t second_registration = {
        2U, "second", &func_temp_driver_ops, &second_temp
    };
    func_sensor_event_t gui_storage[1];
    func_sensor_event_t mqtt_storage[2];
    func_sensor_event_t biz_storage[2];
    func_event_queue_t gui_queue;
    func_event_queue_t mqtt_queue;
    func_event_queue_t biz_queue;
    func_sensor_event_t event;
    func_sensor_event_t latest;
    uint16_t count = 0U;
    uint32_t dropped = 0U;

    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_reset());
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_configure(&first_temp, &first_cfg));
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_configure(&second_temp, &second_cfg));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&gui_queue, gui_storage, 1U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&mqtt_queue, mqtt_storage, 2U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&biz_queue, biz_storage, 2U,
                                                      FUNC_QUEUE_DROP_OLDEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_register(&first_registration));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_register(&second_registration));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(1U, &gui_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(1U, &mqtt_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(1U, &biz_queue));

    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_poll_all(0U));
    TEST_ASSERT_EQ_I32(SNS_ERR_NO_SPACE, func_sensor_poll_all(10U));

    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_get_latest(1U, &latest));
    TEST_ASSERT_EQ_U16(1U, latest.sensor_id);
    TEST_ASSERT_EQ_I32(11000, latest.value);
    TEST_ASSERT_EQ_U32(1U, latest.sequence);
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_get_latest(2U, &latest));
    TEST_ASSERT_EQ_U16(2U, latest.sensor_id);
    TEST_ASSERT_EQ_I32(21000, latest.value);
    TEST_ASSERT_EQ_U32(1U, latest.sequence);

    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_count(&gui_queue, &count));
    TEST_ASSERT_EQ_U16(1U, count);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_dropped(&gui_queue, &dropped));
    TEST_ASSERT_EQ_U32(1U, dropped);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_pop(&gui_queue, &event));
    TEST_ASSERT_EQ_I32(10000, event.value);
    TEST_ASSERT_EQ_U32(0U, event.sequence);

    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_pop(&mqtt_queue, &event));
    TEST_ASSERT_EQ_I32(10000, event.value);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_pop(&mqtt_queue, &event));
    TEST_ASSERT_EQ_I32(11000, event.value);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_pop(&biz_queue, &event));
    TEST_ASSERT_EQ_I32(10000, event.value);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_pop(&biz_queue, &event));
    TEST_ASSERT_EQ_I32(11000, event.value);

    TEST_ASSERT_EQ_U16(1U, first_source.init_calls);
    TEST_ASSERT_EQ_U16(1U, second_source.init_calls);
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_reset());
}

TEST(temp_driver_handles_wrap_and_valid_stale_error_valid_transitions)
{
    temp_source_fixture_t source = {
        { 25000, 0, 0, 26000 },
        { SNS_OK, SNS_ERR_IO, SNS_ERR_TIMEOUT, SNS_OK },
        4U, 0U, 0U
    };
    proto_temp_device_t device = { &temp_source_ops, &source };
    func_temp_cfg_t cfg = make_temp_cfg(&device, 10U, 2U);
    func_temp_t temp;
    func_sensor_event_t event;
    uint8_t ready = UINT8_C(0xFF);

    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_configure(&temp, &cfg));
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_init(&temp));

    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_poll(&temp, UINT32_MAX - 4U, &event, &ready));
    TEST_ASSERT_EQ_U16(1U, ready);
    TEST_ASSERT_EQ_I32(25000, event.value);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_VALID, event.quality);
    TEST_ASSERT_EQ_I32(SNS_OK, event.status);

    ready = UINT8_C(0xFF);
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_poll(&temp, 3U, &event, &ready));
    TEST_ASSERT_EQ_U16(0U, ready);
    TEST_ASSERT_EQ_U16(1U, source.index);

    TEST_ASSERT_EQ_I32(SNS_ERR_IO, func_temp_poll(&temp, 5U, &event, &ready));
    TEST_ASSERT_EQ_U16(1U, ready);
    TEST_ASSERT_EQ_I32(25000, event.value);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_STALE, event.quality);
    TEST_ASSERT_EQ_I32(SNS_ERR_IO, event.status);

    TEST_ASSERT_EQ_I32(SNS_ERR_TIMEOUT, func_temp_poll(&temp, 15U, &event, &ready));
    TEST_ASSERT_EQ_U16(1U, ready);
    TEST_ASSERT_EQ_I32(25000, event.value);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_ERROR, event.quality);
    TEST_ASSERT_EQ_I32(SNS_ERR_TIMEOUT, event.status);

    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_poll(&temp, 25U, &event, &ready));
    TEST_ASSERT_EQ_U16(1U, ready);
    TEST_ASSERT_EQ_I32(26000, event.value);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_VALID, event.quality);
    TEST_ASSERT_EQ_I32(SNS_OK, event.status);
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_deinit(&temp));
}

int main(void)
{
    two_sensor_instances_and_three_subscriber_queues_remain_isolated();
    temp_driver_handles_wrap_and_valid_stale_error_valid_transitions();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
