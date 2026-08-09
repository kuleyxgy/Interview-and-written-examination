#ifndef PROTO_MQTT_H
#define PROTO_MQTT_H

#include <stdint.h>

#include "hal_net.h"
#include "util_status.h"

#define PROTO_MQTT_PACKET_CAPACITY       512U
#define PROTO_MQTT_HOST_CAPACITY         128U
#define PROTO_MQTT_CLIENT_ID_CAPACITY     64U
#define PROTO_MQTT_RECONNECT_INITIAL_MS 1000U
#define PROTO_MQTT_RECONNECT_MAX_MS    30000U

#define PROTO_MQTT_STATE_DISCONNECTED     0U
#define PROTO_MQTT_STATE_SENDING_CONNECT  1U
#define PROTO_MQTT_STATE_WAITING_CONNACK  2U
#define PROTO_MQTT_STATE_CONNECTED        3U

typedef struct {
    uint8_t data[PROTO_MQTT_PACKET_CAPACITY];
    uint16_t length;
    uint16_t sent;
} proto_mqtt_packet_slot_t;

typedef struct {
    hal_net_t *net;
    proto_mqtt_packet_slot_t *slots;
    uint16_t slot_count;
    uint16_t queue_head;
    uint16_t queue_tail;
    uint16_t queue_count;
    uint32_t dropped;
    uint8_t *work_buffer;
    uint16_t work_capacity;
    uint16_t control_length;
    uint16_t control_sent;
    uint16_t receive_length;
    char host[PROTO_MQTT_HOST_CAPACITY];
    char client_id[PROTO_MQTT_CLIENT_ID_CAPACITY];
    uint16_t port;
    uint16_t keepalive_seconds;
    uint32_t connect_timeout_ms;
    uint32_t last_io_ms;
    uint32_t reconnect_at_ms;
    uint32_t reconnect_backoff_ms;
    uint32_t connack_deadline_ms;
    uint8_t configured;
    uint8_t connected;
    uint8_t awaiting_ping_response;
    uint8_t state;
} proto_mqtt_client_t;

sns_status_t proto_mqtt_init(proto_mqtt_client_t *client,
                             hal_net_t *net,
                             proto_mqtt_packet_slot_t *slots,
                             uint16_t slot_count,
                             uint8_t *work_buffer,
                             uint16_t work_capacity);
sns_status_t proto_mqtt_connect(proto_mqtt_client_t *client,
                                const char *host,
                                uint16_t port,
                                const char *client_id,
                                uint16_t keepalive_seconds,
                                uint32_t timeout_ms,
                                uint32_t now_ms);
sns_status_t proto_mqtt_publish_enqueue(proto_mqtt_client_t *client,
                                        const char *topic,
                                        const uint8_t *payload,
                                        uint16_t payload_length);
sns_status_t proto_mqtt_poll(proto_mqtt_client_t *client,
                             uint32_t now_ms,
                             uint32_t budget_ms);
sns_status_t proto_mqtt_close(proto_mqtt_client_t *client);
uint16_t proto_mqtt_pending(const proto_mqtt_client_t *client);
uint32_t proto_mqtt_dropped(const proto_mqtt_client_t *client);
uint8_t proto_mqtt_is_connected(const proto_mqtt_client_t *client);

#endif
