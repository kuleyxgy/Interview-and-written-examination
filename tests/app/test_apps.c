#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "func_app_biz.h"
#include "func_app_gui.h"
#include "func_event_queue.h"
#include "func_sensor_types.h"
#include "proto_display.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    proto_display_record_t records[4];
    uint16_t count;
} display_fixture_t;

static sns_status_t display_fixture_show(void *context,
                                         const proto_display_record_t *record)
{
    display_fixture_t *fixture = context;

    if ((fixture == NULL) || (record == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (fixture->count >= 4U) {
        return SNS_ERR_NO_SPACE;
    }
    fixture->records[fixture->count] = *record;
    fixture->count++;
    return SNS_OK;
}

static const proto_display_ops_t display_fixture_ops = {
    display_fixture_show
};

static func_sensor_event_t make_temp_event(uint16_t sensor_id,
                                           int32_t value,
                                           func_quality_t quality,
                                           uint32_t timestamp_ms)
{
    func_sensor_event_t event;

    event.sensor_id = sensor_id;
    event.kind = FUNC_MEAS_TEMPERATURE;
    event.unit = FUNC_UNIT_MDEG_C;
    event.value = value;
    event.timestamp_ms = timestamp_ms;
    event.sequence = 0U;
    event.quality = quality;
    event.status = (quality == FUNC_QUALITY_VALID) ? SNS_OK : SNS_ERR_IO;
    return event;
}

TEST(gui_formats_negative_fraction_quality_and_poll_budget)
{
    func_sensor_event_t storage[2];
    func_event_queue_t queue;
    display_fixture_t fixture;
    proto_display_t display = { &display_fixture_ops, &fixture };
    func_app_gui_t gui;
    func_sensor_event_t first = make_temp_event(3U, -63, FUNC_QUALITY_STALE, 123U);
    func_sensor_event_t second = make_temp_event(4U, 25125, FUNC_QUALITY_VALID, 456U);

    (void)memset(&fixture, 0, sizeof(fixture));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&queue, storage, 2U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &first));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &second));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_gui_init(&gui, &queue, &display, 1U));

    TEST_ASSERT_EQ_I32(SNS_OK, func_app_gui_poll(&gui, 999U));
    TEST_ASSERT_EQ_U16(1U, fixture.count);
    TEST_ASSERT_EQ_U16(3U, fixture.records[0].sensor_id);
    TEST_ASSERT_STREQ("-0.063", fixture.records[0].value_text);
    TEST_ASSERT_STREQ("C", fixture.records[0].unit_text);
    TEST_ASSERT_STREQ("stale", fixture.records[0].quality_text);
    TEST_ASSERT_EQ_U32(123U, fixture.records[0].timestamp_ms);

    TEST_ASSERT_EQ_I32(SNS_OK, func_app_gui_poll(&gui, 1000U));
    TEST_ASSERT_EQ_U16(2U, fixture.count);
    TEST_ASSERT_EQ_U16(4U, fixture.records[1].sensor_id);
    TEST_ASSERT_STREQ("25.125", fixture.records[1].value_text);
    TEST_ASSERT_STREQ("valid", fixture.records[1].quality_text);
    TEST_ASSERT_EQ_U32(456U, fixture.records[1].timestamp_ms);
}

TEST(biz_tracks_literal_window_alarm_hysteresis_and_stale_quality)
{
    func_sensor_event_t storage[5];
    func_event_queue_t queue;
    int32_t window[3] = { 0, 0, 0 };
    func_app_biz_t biz;
    func_biz_result_t result;
    func_sensor_event_t event;

    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&queue, storage, 5U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_init(&biz, &queue, 7U, window, 3U,
                                                  30000, 28000, 5U));

    event = make_temp_event(8U, 100000, FUNC_QUALITY_VALID, 5U);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &event));

    event = make_temp_event(7U, 29000, FUNC_QUALITY_VALID, 10U);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &event));
    event = make_temp_event(7U, 31000, FUNC_QUALITY_VALID, 20U);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &event));
    event = make_temp_event(7U, 30000, FUNC_QUALITY_VALID, 30U);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &event));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_poll(&biz, 30U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, &result));
    TEST_ASSERT_EQ_U16(7U, result.sensor_id);
    TEST_ASSERT_EQ_I32(29000, result.minimum);
    TEST_ASSERT_EQ_I32(31000, result.maximum);
    TEST_ASSERT_EQ_I32(30000, result.average);
    TEST_ASSERT_EQ_U16(3U, result.sample_count);
    TEST_ASSERT_EQ_U16(1U, result.high_alarm);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_VALID, result.quality);

    event = make_temp_event(7U, 27000, FUNC_QUALITY_VALID, 40U);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &event));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_poll(&biz, 40U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, &result));
    TEST_ASSERT_EQ_I32(27000, result.minimum);
    TEST_ASSERT_EQ_I32(31000, result.maximum);
    TEST_ASSERT_EQ_I32(29333, result.average);
    TEST_ASSERT_EQ_U16(0U, result.high_alarm);

    event = make_temp_event(7U, 99999, FUNC_QUALITY_STALE, 50U);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&queue, &event));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_poll(&biz, 50U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, &result));
    TEST_ASSERT_EQ_I32(27000, result.minimum);
    TEST_ASSERT_EQ_I32(31000, result.maximum);
    TEST_ASSERT_EQ_I32(29333, result.average);
    TEST_ASSERT_EQ_U16(3U, result.sample_count);
    TEST_ASSERT_EQ_U16(0U, result.high_alarm);
    TEST_ASSERT_EQ_I32(FUNC_QUALITY_STALE, result.quality);
    TEST_ASSERT_EQ_U32(50U, result.timestamp_ms);
}

int main(void)
{
    gui_formats_negative_fraction_quality_and_poll_budget();
    biz_tracks_literal_window_alarm_hysteresis_and_stale_quality();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
