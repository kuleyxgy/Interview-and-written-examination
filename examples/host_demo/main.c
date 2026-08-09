#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "func_app_biz.h"
#include "func_app_gui.h"
#include "func_app_mqtt.h"
#include "func_event_queue.h"
#include "func_filter_ema.h"
#include "func_filter_limit.h"
#include "func_filter_ma.h"
#include "func_filter_median.h"
#include "func_sensor.h"
#include "func_temp.h"
#include "hal_i2c.h"
#include "hal_net.h"
#include "host_i2c.h"
#include "host_net.h"
#include "proto_display.h"
#include "proto_i2c_reg.h"
#include "proto_mqtt.h"
#include "proto_temp_tmp75.h"
#include "util_status.h"

#define DEMO_SENSOR_A_ID 1U
#define DEMO_SENSOR_B_ID 2U
#define DEMO_STEP_MS 100U

typedef struct {
    const char *bus_name;
    const uint16_t *register_words;
    const sns_status_t *statuses;
    uint16_t sample_count;
    uint16_t next_sample;
} demo_tmp75_source_t;

static sns_status_t demo_tmp75_transfer(void *ctx,
                                        const uint8_t *write_data,
                                        uint16_t write_length,
                                        uint8_t *read_data,
                                        uint16_t read_capacity,
                                        uint32_t timeout_ms,
                                        uint16_t *transferred)
{
    demo_tmp75_source_t *source = (demo_tmp75_source_t *)ctx;
    uint16_t index;
    uint16_t word;
    uint8_t register_data[2];
    sns_status_t status;

    (void)timeout_ms;
    if ((source == NULL) || (write_data == NULL) || (read_data == NULL) ||
        (transferred == NULL) || (write_length != 1U) ||
        (write_data[0] != PROTO_TEMP_TMP75_TEMPERATURE_REGISTER) ||
        (read_capacity != 2U)) {
        return SNS_ERR_PARAM;
    }
    if (source->next_sample >= source->sample_count) {
        return SNS_ERR_NOT_READY;
    }
    index = source->next_sample;
    source->next_sample++;
    status = source->statuses[index];
    if (status != SNS_OK) {
        (void)printf("[TMP75:%s] injected read error: %s\n",
                     source->bus_name, sns_status_name(status));
        return status;
    }
    word = source->register_words[index];
    register_data[0] = (uint8_t)(word >> 8U);
    register_data[1] = (uint8_t)word;
    read_data[0] = register_data[0];
    read_data[1] = register_data[1];
    *transferred = 2U;
    return SNS_OK;
}

static sns_status_t demo_display_show(void *ctx,
                                      const proto_display_record_t *record)
{
    uint32_t *shown = (uint32_t *)ctx;

    if ((shown == NULL) || (record == NULL)) {
        return SNS_ERR_PARAM;
    }
    (*shown)++;
    (void)printf("[GUI] id=%u value=%s %s quality=%s time=%lu\n",
                 (unsigned int)record->sensor_id,
                 record->value_text,
                 record->unit_text,
                 record->quality_text,
                 (unsigned long)record->timestamp_ms);
    return SNS_OK;
}

static const proto_display_ops_t demo_display_ops = {
    demo_display_show
};

static sns_status_t demo_require(sns_status_t status, const char *operation)
{
    if (status != SNS_OK) {
        (void)fprintf(stderr, "setup failed: %s: %s\n",
                      operation, sns_status_name(status));
    }
    return status;
}

static void demo_print_biz(const char *name, const func_app_biz_t *app)
{
    func_biz_result_t result;
    sns_status_t status = func_app_biz_get_latest(app, &result);

    if (status != SNS_OK) {
        (void)printf("[BIZ:%s] no result: %s\n", name,
                     sns_status_name(status));
        return;
    }
    (void)printf("[BIZ:%s] id=%u min=%ld max=%ld avg=%ld samples=%u "
                 "alarm=%u quality=%u\n",
                 name,
                 (unsigned int)result.sensor_id,
                 (long)result.minimum,
                 (long)result.maximum,
                 (long)result.average,
                 (unsigned int)result.sample_count,
                 (unsigned int)result.high_alarm,
                 (unsigned int)result.quality);
}

static sns_status_t demo_poll_consumers(func_app_gui_t *gui,
                                        func_app_biz_t *biz_a,
                                        func_app_biz_t *biz_b,
                                        uint32_t now_ms)
{
    sns_status_t status;

    status = func_app_gui_poll(gui, now_ms);
    if (status != SNS_OK) {
        return status;
    }
    status = func_app_biz_poll(biz_a, now_ms);
    if (status != SNS_OK) {
        return status;
    }
    status = func_app_biz_poll(biz_b, now_ms);
    if (status != SNS_OK) {
        return status;
    }
    return SNS_OK;
}

static sns_status_t demo_print_new_captures(const host_net_t *network,
                                            uint16_t *printed)
{
    const uint8_t *data;
    const char *kind;
    uint16_t length;
    uint16_t index;
    uint16_t shown;
    sns_status_t status;

    if ((network == NULL) || (printed == NULL)) {
        return SNS_ERR_PARAM;
    }
    while (*printed < host_net_capture_count(network)) {
        status = host_net_capture_at(network, *printed, &data, &length);
        if (status != SNS_OK) {
            return status;
        }
        kind = "OTHER";
        if ((length > 0U) && (data[0] == UINT8_C(0x10))) {
            kind = "CONNECT";
        } else if ((length > 0U) && (data[0] == UINT8_C(0x30))) {
            kind = "PUBLISH";
        } else if ((length > 0U) && (data[0] == UINT8_C(0xC0))) {
            kind = "PINGREQ";
        }
        shown = (length < 40U) ? length : 40U;
        (void)printf("[MQTT:%s] capture=%u len=%u hex=",
                     kind, (unsigned int)*printed, (unsigned int)length);
        for (index = 0U; index < shown; index++) {
            (void)printf("%02X", (unsigned int)data[index]);
            if ((uint16_t)(index + 1U) < shown) {
                (void)printf(" ");
            }
        }
        if (shown < length) {
            (void)printf(" ...");
        }
        (void)printf("\n");
        (*printed)++;
    }
    return SNS_OK;
}

static sns_status_t demo_mqtt_flush(proto_mqtt_client_t *client,
                                    host_net_t *network,
                                    uint16_t *printed,
                                    uint32_t now_ms)
{
    uint16_t attempts = 0U;
    sns_status_t status;

    while ((proto_mqtt_pending(client) > 0U) && (attempts < 16U)) {
        status = proto_mqtt_poll(client, now_ms + attempts, 5U);
        if (status != SNS_OK) {
            return status;
        }
        status = demo_print_new_captures(network, printed);
        if (status != SNS_OK) {
            return status;
        }
        attempts++;
    }
    return (proto_mqtt_pending(client) == 0U) ? SNS_OK : SNS_ERR_TIMEOUT;
}

static sns_status_t demo_mqtt_drain_app(func_app_mqtt_t *app,
                                        proto_mqtt_client_t *client,
                                        host_net_t *network,
                                        uint16_t *printed,
                                        uint32_t now_ms)
{
    uint16_t round;
    sns_status_t status;

    for (round = 0U; round < 8U; round++) {
        status = func_app_mqtt_poll(app, now_ms);
        (void)printf("[MQTT:queue] app_status=%s pending=%u dropped=%lu\n",
                     sns_status_name(status),
                     (unsigned int)proto_mqtt_pending(client),
                     (unsigned long)proto_mqtt_dropped(client));
        if ((status != SNS_OK) && (status != SNS_ERR_NO_SPACE)) {
            return status;
        }
        if (demo_mqtt_flush(client, network, printed,
                            now_ms + (uint32_t)round * 20U) != SNS_OK) {
            return SNS_ERR_IO;
        }
        if (status == SNS_OK) {
            return SNS_OK;
        }
    }
    return SNS_ERR_TIMEOUT;
}

int main(void)
{
    static const uint16_t sensor_a_words[] = {
        UINT16_C(0x1900), UINT16_C(0x1900), UINT16_C(0x1900),
        UINT16_C(0x1E00), UINT16_C(0x1F00), UINT16_C(0x1900)
    };
    static const sns_status_t sensor_a_statuses[] = {
        SNS_OK, SNS_ERR_IO, SNS_ERR_TIMEOUT, SNS_OK, SNS_OK, SNS_OK
    };
    static const uint16_t sensor_b_words[] = {
        UINT16_C(0xFB00), UINT16_C(0xFC00), UINT16_C(0xFD00),
        UINT16_C(0xFE00), UINT16_C(0xFF00), UINT16_C(0x0000)
    };
    static const sns_status_t sensor_b_statuses[] = {
        SNS_OK, SNS_OK, SNS_OK, SNS_OK, SNS_OK, SNS_OK
    };
    static const uint8_t accepted_connack[] = {
        UINT8_C(0x20), UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x00)
    };
    demo_tmp75_source_t source_a = {
        "bus-A", sensor_a_words, sensor_a_statuses,
        (uint16_t)(sizeof(sensor_a_words) / sizeof(sensor_a_words[0])), 0U
    };
    demo_tmp75_source_t source_b = {
        "bus-B", sensor_b_words, sensor_b_statuses,
        (uint16_t)(sizeof(sensor_b_words) / sizeof(sensor_b_words[0])), 0U
    };
    host_i2c_bus_t bus_a;
    host_i2c_bus_t bus_b;
    hal_i2c_t i2c_hal_a;
    hal_i2c_t i2c_hal_b;
    proto_i2c_device_t i2c_device_a;
    proto_i2c_device_t i2c_device_b;
    proto_temp_tmp75_t tmp75_a;
    proto_temp_tmp75_t tmp75_b;
    proto_temp_device_t device_a;
    proto_temp_device_t device_b;
    func_filter_ma_cfg_t ma_cfg = { 2U };
    func_filter_ma_state_t ma_state;
    func_filter_limit_cfg_t limit_cfg = { -55000, 125000, 2000 };
    func_filter_limit_state_t limit_state;
    func_filter_instance_t filters_a[] = {
        { &func_filter_ma_ops, &ma_state, (uint16_t)sizeof(ma_state), &ma_cfg },
        { &func_filter_limit_ops, &limit_state,
          (uint16_t)sizeof(limit_state), &limit_cfg }
    };
    func_filter_median_cfg_t median_cfg = { 3U };
    func_filter_median_state_t median_state;
    func_filter_ema_cfg_t ema_cfg = { 16384U };
    func_filter_ema_state_t ema_state;
    func_filter_instance_t filters_b[] = {
        { &func_filter_median_ops, &median_state,
          (uint16_t)sizeof(median_state), &median_cfg },
        { &func_filter_ema_ops, &ema_state, (uint16_t)sizeof(ema_state), &ema_cfg }
    };
    func_temp_cfg_t cfg_a;
    func_temp_cfg_t cfg_b;
    func_temp_t temp_a;
    func_temp_t temp_b;
    func_sensor_registration_t registration_a = {
        DEMO_SENSOR_A_ID, "TMP75-A", &func_temp_driver_ops, &temp_a
    };
    func_sensor_registration_t registration_b = {
        DEMO_SENSOR_B_ID, "TMP75-B", &func_temp_driver_ops, &temp_b
    };
    func_sensor_core_t core;
    func_sensor_event_t gui_storage[5];
    func_sensor_event_t mqtt_storage[5];
    func_sensor_event_t biz_a_storage[3];
    func_sensor_event_t biz_b_storage[2];
    func_event_queue_t gui_queue;
    func_event_queue_t mqtt_queue;
    func_event_queue_t biz_a_queue;
    func_event_queue_t biz_b_queue;
    uint32_t display_count = 0U;
    proto_display_t display = { &demo_display_ops, &display_count };
    func_app_gui_t gui;
    int32_t biz_a_window[3];
    int32_t biz_b_window[3];
    func_app_biz_t biz_a;
    func_app_biz_t biz_b;
    host_net_t network;
    hal_net_t network_hal;
    proto_mqtt_packet_slot_t mqtt_slots[2];
    uint8_t mqtt_work[256];
    proto_mqtt_client_t mqtt_client;
    func_app_mqtt_t mqtt_app;
    uint32_t now_ms;
    uint32_t gui_dropped;
    uint32_t mqtt_event_dropped;
    sns_status_t status;
    uint16_t printed_captures = 0U;
    uint16_t step;

    if ((demo_require(host_i2c_bus_init(&bus_a), "I2C bus A") != SNS_OK) ||
        (demo_require(host_i2c_bus_init(&bus_b), "I2C bus B") != SNS_OK) ||
        (demo_require(host_i2c_bus_bind(&bus_a, &i2c_hal_a),
                      "bind I2C A") != SNS_OK) ||
        (demo_require(host_i2c_bus_bind(&bus_b, &i2c_hal_b),
                      "bind I2C B") != SNS_OK) ||
        (demo_require(host_i2c_bus_register(&bus_a, UINT16_C(0x48),
                                             demo_tmp75_transfer, &source_a),
                      "TMP75 on bus A") != SNS_OK) ||
        (demo_require(host_i2c_bus_register(&bus_b, UINT16_C(0x48),
                                             demo_tmp75_transfer, &source_b),
                      "TMP75 on bus B") != SNS_OK) ||
        (demo_require(proto_i2c_device_init(&i2c_device_a, &i2c_hal_a,
                                             UINT16_C(0x48), 7U, 5U),
                      "I2C device A") != SNS_OK) ||
        (demo_require(proto_i2c_device_init(&i2c_device_b, &i2c_hal_b,
                                             UINT16_C(0x48), 7U, 5U),
                      "I2C device B") != SNS_OK) ||
        (demo_require(proto_temp_tmp75_init(&tmp75_a, &i2c_device_a),
                      "TMP75 protocol A") != SNS_OK) ||
        (demo_require(proto_temp_tmp75_init(&tmp75_b, &i2c_device_b),
                      "TMP75 protocol B") != SNS_OK) ||
        (demo_require(proto_temp_tmp75_bind(&tmp75_a, &device_a),
                      "temperature device A") != SNS_OK) ||
        (demo_require(proto_temp_tmp75_bind(&tmp75_b, &device_b),
                      "temperature device B") != SNS_OK)) {
        return 1;
    }

    (void)memset(&cfg_a, 0, sizeof(cfg_a));
    cfg_a.source = &device_a;
    cfg_a.sample_period_ms = DEMO_STEP_MS;
    cfg_a.filters.items = filters_a;
    cfg_a.filters.count = (uint8_t)(sizeof(filters_a) / sizeof(filters_a[0]));
    cfg_a.force_publish_period_ms = DEMO_STEP_MS;
    cfg_a.error_threshold = 2U;
    (void)memset(&cfg_b, 0, sizeof(cfg_b));
    cfg_b.source = &device_b;
    cfg_b.sample_period_ms = DEMO_STEP_MS;
    cfg_b.filters.items = filters_b;
    cfg_b.filters.count = (uint8_t)(sizeof(filters_b) / sizeof(filters_b[0]));
    cfg_b.force_publish_period_ms = DEMO_STEP_MS;
    cfg_b.error_threshold = 2U;

    if ((demo_require(func_sensor_core_init(&core), "sensor core") != SNS_OK) ||
        (demo_require(func_temp_configure(&temp_a, &cfg_a), "TMP75-A") != SNS_OK) ||
        (demo_require(func_temp_configure(&temp_b, &cfg_b), "TMP75-B") != SNS_OK) ||
        (demo_require(func_event_queue_init(&gui_queue, gui_storage, 5U,
                                             FUNC_QUEUE_DROP_NEWEST),
                      "GUI queue") != SNS_OK) ||
        (demo_require(func_event_queue_init(&mqtt_queue, mqtt_storage, 5U,
                                             FUNC_QUEUE_DROP_NEWEST),
                      "MQTT event queue") != SNS_OK) ||
        (demo_require(func_event_queue_init(&biz_a_queue, biz_a_storage, 3U,
                                             FUNC_QUEUE_DROP_OLDEST),
                      "business A queue") != SNS_OK) ||
        (demo_require(func_event_queue_init(&biz_b_queue, biz_b_storage, 2U,
                                             FUNC_QUEUE_DROP_OLDEST),
                      "business B queue") != SNS_OK) ||
        (demo_require(func_sensor_register(&core, &registration_a),
                      "register A") != SNS_OK) ||
        (demo_require(func_sensor_register(&core, &registration_b),
                      "register B") != SNS_OK)) {
        return 1;
    }

    if ((demo_require(func_sensor_subscribe(&core, DEMO_SENSOR_A_ID, &gui_queue),
                      "A to GUI") != SNS_OK) ||
        (demo_require(func_sensor_subscribe(&core, DEMO_SENSOR_B_ID, &gui_queue),
                      "B to GUI") != SNS_OK) ||
        (demo_require(func_sensor_subscribe(&core, DEMO_SENSOR_A_ID, &mqtt_queue),
                      "A to MQTT") != SNS_OK) ||
        (demo_require(func_sensor_subscribe(&core, DEMO_SENSOR_B_ID, &mqtt_queue),
                      "B to MQTT") != SNS_OK) ||
        (demo_require(func_sensor_subscribe(&core, DEMO_SENSOR_A_ID, &biz_a_queue),
                      "A to business") != SNS_OK) ||
        (demo_require(func_sensor_subscribe(&core, DEMO_SENSOR_B_ID, &biz_b_queue),
                      "B to business") != SNS_OK) ||
        (demo_require(func_app_gui_init(&gui, &gui_queue, &display, 8U),
                      "GUI app") != SNS_OK) ||
        (demo_require(func_app_biz_init(&biz_a, &biz_a_queue, DEMO_SENSOR_A_ID,
                                         biz_a_window, 3U, 29000, 28000, 8U),
                      "business A") != SNS_OK) ||
        (demo_require(func_app_biz_init(&biz_b, &biz_b_queue, DEMO_SENSOR_B_ID,
                                         biz_b_window, 3U, 0, -1000, 8U),
                      "business B") != SNS_OK)) {
        (void)func_sensor_reset(&core);
        return 1;
    }
    if ((demo_require(host_net_init(&network), "network simulation") != SNS_OK) ||
        (demo_require(host_net_bind(&network, &network_hal),
                      "network binding") != SNS_OK) ||
        (demo_require(proto_mqtt_init(&mqtt_client, &network_hal, mqtt_slots,
                                       (uint16_t)(sizeof(mqtt_slots) /
                                                  sizeof(mqtt_slots[0])),
                                       mqtt_work,
                                       (uint16_t)sizeof(mqtt_work)),
                      "MQTT client") != SNS_OK) ||
        (demo_require(func_app_mqtt_init(&mqtt_app, &mqtt_queue, &mqtt_client,
                                          "demo/temperature", 8U),
                      "MQTT app") != SNS_OK) ||
        (demo_require(proto_mqtt_connect(&mqtt_client, "demo-broker", 1883U,
                                          "sensor-demo", 10U, 50U, 0U),
                      "MQTT transport connect") != SNS_OK) ||
        (demo_require(proto_mqtt_poll(&mqtt_client, 0U, 5U),
                      "MQTT CONNECT send") != SNS_OK) ||
        (demo_require(demo_print_new_captures(&network, &printed_captures),
                      "CONNECT capture") != SNS_OK) ||
        (demo_require(host_net_receive_push(&network, accepted_connack,
                                             (uint16_t)sizeof(accepted_connack)),
                      "CONNACK injection") != SNS_OK) ||
        (demo_require(proto_mqtt_poll(&mqtt_client, 1U, 5U),
                      "CONNACK processing") != SNS_OK) ||
        (proto_mqtt_is_connected(&mqtt_client) == 0U)) {
        (void)func_sensor_reset(&core);
        return 1;
    }
    (void)printf("[MQTT:CONNACK] injected hex=20 02 00 00 connected=1\n");

    (void)printf("dual TMP75 demo: two host I2C buses, same 0x48 address, "
                 "filters, and consumers\n");
    for (step = 0U; step < 3U; step++) {
        now_ms = (uint32_t)step * DEMO_STEP_MS;
        status = func_sensor_poll_all(&core, now_ms);
        (void)printf("[CORE] time=%lu status=%s\n",
                     (unsigned long)now_ms, sns_status_name(status));
    }
    if ((func_event_queue_dropped(&gui_queue, &gui_dropped) != SNS_OK) ||
        (func_event_queue_dropped(&mqtt_queue, &mqtt_event_dropped) != SNS_OK)) {
        (void)func_sensor_reset(&core);
        return 1;
    }
    (void)printf("[FANOUT] before drain GUI_dropped=%lu MQTT_event_dropped=%lu\n",
                 (unsigned long)gui_dropped,
                 (unsigned long)mqtt_event_dropped);
    if ((demo_poll_consumers(&gui, &biz_a, &biz_b, 200U) != SNS_OK) ||
        (demo_mqtt_drain_app(&mqtt_app, &mqtt_client, &network,
                             &printed_captures, 200U) != SNS_OK)) {
        (void)func_sensor_reset(&core);
        return 1;
    }
    demo_print_biz("A", &biz_a);
    demo_print_biz("B", &biz_b);

    for (step = 3U; step < 6U; step++) {
        now_ms = (uint32_t)step * DEMO_STEP_MS;
        status = func_sensor_poll_all(&core, now_ms);
        (void)printf("[CORE] recovery time=%lu status=%s\n",
                     (unsigned long)now_ms, sns_status_name(status));
        if (demo_poll_consumers(&gui, &biz_a, &biz_b, now_ms) != SNS_OK) {
            (void)func_sensor_reset(&core);
            return 1;
        }
        if (step == 4U) {
            status = func_app_mqtt_poll(&mqtt_app, now_ms);
            if (status != SNS_OK) {
                (void)func_sensor_reset(&core);
                return 1;
            }
            status = network_hal.ops->close(network_hal.ctx);
            if (status != SNS_OK) {
                (void)func_sensor_reset(&core);
                return 1;
            }
            status = proto_mqtt_poll(&mqtt_client, now_ms, 5U);
            (void)printf("[MQTT:disconnect] send_status=%s connected=%u "
                         "queued=%u\n",
                         sns_status_name(status),
                         (unsigned int)proto_mqtt_is_connected(&mqtt_client),
                         (unsigned int)proto_mqtt_pending(&mqtt_client));
            if ((status != SNS_ERR_STATE) ||
                (proto_mqtt_pending(&mqtt_client) == 0U) ||
                (proto_mqtt_poll(&mqtt_client, 1400U, 5U) != SNS_OK) ||
                (proto_mqtt_poll(&mqtt_client, 1401U, 5U) != SNS_OK) ||
                (demo_print_new_captures(&network, &printed_captures) != SNS_OK) ||
                (host_net_receive_push(&network, accepted_connack,
                                        (uint16_t)sizeof(accepted_connack)) != SNS_OK) ||
                (proto_mqtt_poll(&mqtt_client, 1402U, 5U) != SNS_OK) ||
                (demo_mqtt_flush(&mqtt_client, &network, &printed_captures,
                                 1403U) != SNS_OK)) {
                (void)func_sensor_reset(&core);
                return 1;
            }
        } else if (demo_mqtt_drain_app(&mqtt_app, &mqtt_client, &network,
                                        &printed_captures, now_ms) != SNS_OK) {
            (void)func_sensor_reset(&core);
            return 1;
        }
        demo_print_biz("A", &biz_a);
        demo_print_biz("B", &biz_b);
    }
    (void)printf("[SUMMARY] GUI_records=%lu MQTT_pending=%u MQTT_dropped=%lu\n",
                 (unsigned long)display_count,
                 (unsigned int)proto_mqtt_pending(&mqtt_client),
                 (unsigned long)proto_mqtt_dropped(&mqtt_client));

    if ((demo_require(proto_mqtt_close(&mqtt_client), "MQTT close") != SNS_OK) ||
        (demo_require(func_sensor_reset(&core), "sensor reset") != SNS_OK)) {
        return 1;
    }
    return 0;
}
