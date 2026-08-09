#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "func_app_biz.h"
#include "func_app_gui.h"
#include "func_app_mqtt.h"
#include "func_filter_limit.h"
#include "func_filter_ma.h"
#include "func_sensor.h"
#include "func_temp.h"
#include "hal_i2c.h"
#include "hal_net.h"
#include "host_i2c.h"
#include "host_net.h"
#include "proto_display.h"
#include "proto_i2c_reg.h"
#include "proto_mqtt.h"
#include "proto_temp.h"
#include "proto_temp_tmp75.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    uint16_t words[2];
    uint16_t next;
} integration_tmp75_t;

typedef struct {
    proto_display_record_t records[4];
    uint16_t count;
} integration_display_t;

static sns_status_t integration_i2c_transfer(void *context,
                                              const uint8_t *write_data,
                                              uint16_t write_length,
                                              uint8_t *read_data,
                                              uint16_t read_capacity,
                                              uint32_t timeout_ms,
                                              uint16_t *transferred)
{
    integration_tmp75_t *fixture = context;
    uint16_t word;
    (void)timeout_ms;

    if ((fixture == NULL) || (write_data == NULL) || (write_length != 1U) ||
        (write_data[0] != UINT8_C(0x00)) || (read_data == NULL) ||
        (read_capacity < 2U) || (transferred == NULL) || (fixture->next >= 2U)) {
        return SNS_ERR_PARAM;
    }
    word = fixture->words[fixture->next];
    fixture->next++;
    read_data[0] = (uint8_t)(word >> 8U);
    read_data[1] = (uint8_t)word;
    *transferred = 2U;
    return SNS_OK;
}

static sns_status_t integration_display_show(void *context,
                                               const proto_display_record_t *record)
{
    integration_display_t *display = context;
    if ((display == NULL) || (record == NULL) || (display->count >= 4U)) {
        return SNS_ERR_NO_SPACE;
    }
    display->records[display->count] = *record;
    display->count++;
    return SNS_OK;
}

static const proto_display_ops_t integration_display_ops = {
    integration_display_show
};

static void assert_publish(const host_net_t *host, uint16_t capture_index,
                           const uint8_t *header, uint16_t header_length,
                           const char *payload, uint16_t payload_length,
                           uint16_t packet_length)
{
    const uint8_t *packet = NULL;
    uint16_t length = 0U;

    TEST_ASSERT_EQ_I32(SNS_OK,
                       host_net_capture_at(host, capture_index, &packet, &length));
    TEST_ASSERT_EQ_U16(packet_length, length);
    TEST_ASSERT_TRUE(memcmp(header, packet, header_length) == 0);
    TEST_ASSERT_TRUE(memcmp(payload, &packet[header_length], payload_length) == 0);
}

TEST(two_real_tmp75_pipelines_fan_out_and_isolate_one_full_queue)
{
    static const uint8_t publish_a_header[] = {
        UINT8_C(0x30), UINT8_C(0x72), UINT8_C(0x00), UINT8_C(0x07),
        (uint8_t)'q', (uint8_t)'a', (uint8_t)'/', (uint8_t)'t',
        (uint8_t)'e', (uint8_t)'m', (uint8_t)'p'
    };
    static const uint8_t publish_b_header[] = {
        UINT8_C(0x30), UINT8_C(0x73), UINT8_C(0x00), UINT8_C(0x07),
        (uint8_t)'q', (uint8_t)'a', (uint8_t)'/', (uint8_t)'t',
        (uint8_t)'e', (uint8_t)'m', (uint8_t)'p'
    };
    static const char publish_a_payload[] =
        "{\"sensor_id\":1,\"kind\":\"temperature\",\"value_mdeg_c\":25000,"
        "\"quality\":\"valid\",\"timestamp_ms\":0,\"sequence\":0}";
    static const char publish_b_payload[] =
        "{\"sensor_id\":2,\"kind\":\"temperature\",\"value_mdeg_c\":-55000,"
        "\"quality\":\"valid\",\"timestamp_ms\":0,\"sequence\":0}";
    static const uint8_t connack[] = {
        UINT8_C(0x20), UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x00)
    };
    integration_tmp75_t raw_a = { { UINT16_C(0x1900), UINT16_C(0x1B00) }, 0U };
    integration_tmp75_t raw_b = { { UINT16_C(0xC900), UINT16_C(0xCA00) }, 0U };
    host_i2c_bus_t bus_a;
    host_i2c_bus_t bus_b;
    hal_i2c_t hal_a;
    hal_i2c_t hal_b;
    proto_i2c_device_t i2c_a;
    proto_i2c_device_t i2c_b;
    proto_temp_tmp75_t tmp75_a;
    proto_temp_tmp75_t tmp75_b;
    proto_temp_device_t source_a;
    proto_temp_device_t source_b;
    func_filter_ma_cfg_t ma_cfg = { 2U };
    func_filter_ma_state_t ma_state;
    func_filter_limit_cfg_t limit_cfg = { -55000, 125000, 500 };
    func_filter_limit_state_t limit_state;
    func_filter_instance_t filter_a = {
        &func_filter_ma_ops, &ma_state, (uint16_t)sizeof(ma_state), &ma_cfg
    };
    func_filter_instance_t filter_b = {
        &func_filter_limit_ops, &limit_state, (uint16_t)sizeof(limit_state), &limit_cfg
    };
    func_temp_cfg_t cfg_a;
    func_temp_cfg_t cfg_b;
    func_temp_t temp_a;
    func_temp_t temp_b;
    func_sensor_registration_t registration_a = {
        1U, "tmp75-a", &func_temp_driver_ops, &temp_a
    };
    func_sensor_registration_t registration_b = {
        2U, "tmp75-b", &func_temp_driver_ops, &temp_b
    };
    func_sensor_core_t core;
    func_sensor_event_t gui_storage[2];
    func_sensor_event_t mqtt_storage[4];
    func_sensor_event_t biz_storage[4];
    func_event_queue_t gui_queue;
    func_event_queue_t mqtt_queue;
    func_event_queue_t biz_queue;
    integration_display_t display_capture;
    proto_display_t display = { &integration_display_ops, &display_capture };
    func_app_gui_t gui;
    host_net_t host_net;
    hal_net_t hal_net;
    proto_mqtt_packet_slot_t mqtt_slots[4];
    uint8_t mqtt_work[128];
    proto_mqtt_client_t mqtt_client;
    func_app_mqtt_t mqtt_app;
    int32_t biz_window_a[2];
    int32_t biz_window_b[2];
    func_biz_sensor_state_t biz_states[2];
    func_app_biz_t biz;
    func_sensor_event_t latest;
    func_sensor_event_t dummy;
    func_biz_result_t result;
    uint32_t dropped = 0U;
    uint16_t count = 0U;

    (void)memset(&cfg_a, 0, sizeof(cfg_a));
    (void)memset(&cfg_b, 0, sizeof(cfg_b));
    (void)memset(&display_capture, 0, sizeof(display_capture));
    (void)memset(&dummy, 0, sizeof(dummy));
    cfg_a.sample_period_ms = 10U;
    cfg_a.filters.items = &filter_a;
    cfg_a.filters.count = 1U;
    cfg_a.publish_change_mdeg_c = 0;
    cfg_a.error_threshold = 2U;
    cfg_b.sample_period_ms = 10U;
    cfg_b.filters.items = &filter_b;
    cfg_b.filters.count = 1U;
    cfg_b.publish_change_mdeg_c = 0;
    cfg_b.error_threshold = 2U;

    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_init(&bus_a));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_init(&bus_b));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_bind(&bus_a, &hal_a));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_bind(&bus_b, &hal_b));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&bus_a, UINT8_C(0x48),
                                                      integration_i2c_transfer, &raw_a));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&bus_b, UINT8_C(0x48),
                                                      integration_i2c_transfer, &raw_b));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_i2c_device_init(&i2c_a, &hal_a, UINT8_C(0x48), 7U, 20U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_i2c_device_init(&i2c_b, &hal_b, UINT8_C(0x48), 7U, 20U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_init(&tmp75_a, &i2c_a));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_init(&tmp75_b, &i2c_b));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_bind(&tmp75_a, &source_a));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_bind(&tmp75_b, &source_b));
    cfg_a.source = &source_a;
    cfg_b.source = &source_b;
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_configure(&temp_a, &cfg_a));
    TEST_ASSERT_EQ_I32(SNS_OK, func_temp_configure(&temp_b, &cfg_b));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_core_init(&core));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_register(&core, &registration_a));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_register(&core, &registration_b));

    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&gui_queue, gui_storage, 2U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&mqtt_queue, mqtt_storage, 4U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_init(&biz_queue, biz_storage, 4U,
                                                      FUNC_QUEUE_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(&core, 1U, &gui_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(&core, 2U, &gui_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(&core, 1U, &mqtt_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(&core, 2U, &mqtt_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(&core, 1U, &biz_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_subscribe(&core, 2U, &biz_queue));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_gui_init(&gui, &gui_queue, &display, 4U));

    TEST_ASSERT_EQ_I32(SNS_OK, host_net_init(&host_net));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_bind(&host_net, &hal_net));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_init(&mqtt_client, &hal_net, mqtt_slots, 4U,
                                                mqtt_work, (uint16_t)sizeof(mqtt_work)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&mqtt_client, "broker", 1883U,
                                                   "integration", 10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&mqtt_client, 0U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host_net, connack,
                                                      (uint16_t)sizeof(connack)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&mqtt_client, 1U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_mqtt_init(&mqtt_app, &mqtt_queue, &mqtt_client,
                                                   "qa/temp", 4U));

    TEST_ASSERT_EQ_I32(SNS_OK, func_biz_sensor_state_init(&biz_states[0], 1U,
                                                           biz_window_a, 2U, 30000, 29000));
    TEST_ASSERT_EQ_I32(SNS_OK, func_biz_sensor_state_init(&biz_states[1], 2U,
                                                           biz_window_b, 2U, 0, -1000));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_init(&biz, &biz_queue, biz_states, 2U, 4U));

    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_poll_all(&core, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_get_latest(&core, 1U, &latest));
    TEST_ASSERT_EQ_I32(25000, latest.value);
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_get_latest(&core, 2U, &latest));
    TEST_ASSERT_EQ_I32(-55000, latest.value);
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_gui_poll(&gui, 0U));
    TEST_ASSERT_EQ_U16(2U, display_capture.count);
    TEST_ASSERT_STREQ("25.000", display_capture.records[0].value_text);
    TEST_ASSERT_STREQ("-55.000", display_capture.records[1].value_text);
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_poll(&biz, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, 1U, &result));
    TEST_ASSERT_EQ_I32(25000, result.average);
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, 2U, &result));
    TEST_ASSERT_EQ_I32(-55000, result.average);
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_mqtt_poll(&mqtt_app, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&mqtt_client, 2U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&mqtt_client, 3U, 2U));
    assert_publish(&host_net, 1U, publish_a_header, 11U, publish_a_payload, 105U, 116U);
    assert_publish(&host_net, 2U, publish_b_header, 11U, publish_b_payload, 106U, 117U);

    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&gui_queue, &dummy));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_push(&gui_queue, &dummy));
    TEST_ASSERT_EQ_I32(SNS_ERR_NO_SPACE, func_sensor_poll_all(&core, 10U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_dropped(&gui_queue, &dropped));
    TEST_ASSERT_EQ_U32(2U, dropped);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_count(&mqtt_queue, &count));
    TEST_ASSERT_EQ_U16(2U, count);
    TEST_ASSERT_EQ_I32(SNS_OK, func_event_queue_count(&biz_queue, &count));
    TEST_ASSERT_EQ_U16(2U, count);
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_poll(&biz, 10U));
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, 1U, &result));
    TEST_ASSERT_EQ_I32(25500, result.average);
    TEST_ASSERT_EQ_I32(SNS_OK, func_app_biz_get_latest(&biz, 2U, &result));
    TEST_ASSERT_EQ_I32(-54750, result.average);
    TEST_ASSERT_EQ_I32(SNS_OK, func_sensor_reset(&core));
}

int main(void)
{
    two_real_tmp75_pipelines_fan_out_and_isolate_one_full_queue();
    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }
    return 0;
}
