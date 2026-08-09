#include "func_app_mqtt.h"

#include <stddef.h>
#include <stdio.h>

static const char *func_app_mqtt_quality(func_quality_t quality)
{
    switch (quality) {
    case FUNC_QUALITY_VALID: return "valid";
    case FUNC_QUALITY_STALE: return "stale";
    case FUNC_QUALITY_ERROR: return "error";
    default: return "unknown";
    }
}

sns_status_t func_app_mqtt_init(func_app_mqtt_t *app,
                                func_event_queue_t *queue,
                                proto_mqtt_client_t *client,
                                const char *topic,
                                uint16_t max_events_per_poll)
{
    uint16_t ignored;

    if ((app == NULL) || (client == NULL) || (topic == NULL) ||
        (topic[0] == '\0') || (max_events_per_poll == 0U) ||
        (func_event_queue_count(queue, &ignored) != SNS_OK)) {
        return SNS_ERR_PARAM;
    }
    app->queue = queue;
    app->client = client;
    app->topic = topic;
    app->max_events_per_poll = max_events_per_poll;
    app->payload[0] = '\0';
    return SNS_OK;
}

sns_status_t func_app_mqtt_poll(func_app_mqtt_t *app, uint32_t now_ms)
{
    uint16_t index;
    func_sensor_event_t event;
    int written;
    sns_status_t status;

    (void)now_ms;
    if ((app == NULL) || (app->queue == NULL) || (app->client == NULL)) {
        return SNS_ERR_PARAM;
    }
    for (index = 0U; index < app->max_events_per_poll; index++) {
        status = func_event_queue_pop(app->queue, &event);
        if (status == SNS_ERR_NOT_FOUND) {
            return SNS_OK;
        }
        if (status != SNS_OK) {
            return status;
        }
        if ((event.kind != FUNC_MEAS_TEMPERATURE) ||
            (event.unit != FUNC_UNIT_MDEG_C)) {
            return SNS_ERR_UNSUPPORTED;
        }
        written = snprintf(app->payload, sizeof(app->payload),
                           "{\"sensor_id\":%u,\"kind\":\"temperature\","
                           "\"value_mdeg_c\":%ld,\"quality\":\"%s\","
                           "\"timestamp_ms\":%lu,\"sequence\":%lu}",
                           (unsigned int)event.sensor_id, (long)event.value,
                           func_app_mqtt_quality(event.quality),
                           (unsigned long)event.timestamp_ms,
                           (unsigned long)event.sequence);
        if ((written < 0) || ((size_t)written >= sizeof(app->payload))) {
            return SNS_ERR_NO_SPACE;
        }
        status = proto_mqtt_publish_enqueue(app->client, app->topic,
                                             (const uint8_t *)app->payload,
                                             (uint16_t)written);
        if (status != SNS_OK) {
            return status;
        }
    }
    return SNS_OK;
}
