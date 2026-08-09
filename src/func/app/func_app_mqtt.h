#ifndef FUNC_APP_MQTT_H
#define FUNC_APP_MQTT_H

#include <stdint.h>

#include "func_event_queue.h"
#include "proto_mqtt.h"

#define FUNC_APP_MQTT_PAYLOAD_CAPACITY 192U

typedef struct {
    func_event_queue_t *queue;
    proto_mqtt_client_t *client;
    const char *topic;
    uint16_t max_events_per_poll;
    char payload[FUNC_APP_MQTT_PAYLOAD_CAPACITY];
} func_app_mqtt_t;

sns_status_t func_app_mqtt_init(func_app_mqtt_t *app,
                                func_event_queue_t *queue,
                                proto_mqtt_client_t *client,
                                const char *topic,
                                uint16_t max_events_per_poll);
sns_status_t func_app_mqtt_poll(func_app_mqtt_t *app, uint32_t now_ms);

#endif
