#include "proto_mqtt.h"

#include <stddef.h>
#include <string.h>

static uint8_t proto_mqtt_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((uint32_t)(now_ms - deadline_ms) < UINT32_C(0x80000000)) ? 1U : 0U;
}

static sns_status_t proto_mqtt_encode_remaining(uint32_t value,
                                                 uint8_t *output,
                                                 uint16_t capacity,
                                                 uint16_t *encoded_length)
{
    uint16_t used = 0U;

    if ((output == NULL) || (encoded_length == NULL) ||
        (value > UINT32_C(268435455))) {
        return SNS_ERR_PARAM;
    }

    do {
        uint8_t byte;

        if (used >= capacity) {
            return SNS_ERR_NO_SPACE;
        }
        byte = (uint8_t)(value % 128U);
        value /= 128U;
        if (value != 0U) {
            byte = (uint8_t)(byte | UINT8_C(0x80));
        }
        output[used] = byte;
        used++;
    } while (value != 0U);

    *encoded_length = used;
    return SNS_OK;
}

static sns_status_t proto_mqtt_copy_text(char *destination,
                                          uint16_t capacity,
                                          const char *source)
{
    size_t length;

    if ((destination == NULL) || (source == NULL) || (capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    length = strlen(source);
    if ((length == 0U) || (length >= capacity) || (length > UINT16_MAX)) {
        return SNS_ERR_NO_SPACE;
    }
    (void)memcpy(destination, source, length + 1U);
    return SNS_OK;
}

static sns_status_t proto_mqtt_validate_text(const char *text,
                                              uint16_t capacity)
{
    size_t length;

    if ((text == NULL) || (capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    length = strlen(text);
    if ((length == 0U) || (length >= capacity) || (length > UINT16_MAX)) {
        return SNS_ERR_NO_SPACE;
    }
    return SNS_OK;
}

static sns_status_t proto_mqtt_encode_connect(proto_mqtt_client_t *client)
{
    size_t client_id_length = strlen(client->client_id);
    uint32_t remaining_length = (uint32_t)client_id_length + 12U;
    uint16_t remaining_bytes = 0U;
    uint16_t offset = 1U;
    sns_status_t status;

    status = proto_mqtt_encode_remaining(remaining_length,
                                          &client->work_buffer[offset],
                                          (uint16_t)(client->work_capacity - offset),
                                          &remaining_bytes);
    if (status != SNS_OK) {
        return status;
    }
    offset = (uint16_t)(offset + remaining_bytes);
    if ((uint32_t)offset + remaining_length > client->work_capacity) {
        return SNS_ERR_NO_SPACE;
    }

    client->work_buffer[0] = UINT8_C(0x10);
    client->work_buffer[offset++] = 0U;
    client->work_buffer[offset++] = 4U;
    client->work_buffer[offset++] = (uint8_t)'M';
    client->work_buffer[offset++] = (uint8_t)'Q';
    client->work_buffer[offset++] = (uint8_t)'T';
    client->work_buffer[offset++] = (uint8_t)'T';
    client->work_buffer[offset++] = 4U;
    client->work_buffer[offset++] = UINT8_C(0x02);
    client->work_buffer[offset++] = (uint8_t)(client->keepalive_seconds >> 8U);
    client->work_buffer[offset++] = (uint8_t)client->keepalive_seconds;
    client->work_buffer[offset++] = (uint8_t)(client_id_length >> 8U);
    client->work_buffer[offset++] = (uint8_t)client_id_length;
    (void)memcpy(&client->work_buffer[offset], client->client_id, client_id_length);
    offset = (uint16_t)(offset + (uint16_t)client_id_length);
    client->control_length = offset;
    client->control_sent = 0U;
    return SNS_OK;
}

static void proto_mqtt_schedule_reconnect(proto_mqtt_client_t *client,
                                           uint32_t now_ms)
{
    uint32_t delay = client->reconnect_backoff_ms;

    if (delay == 0U) {
        delay = PROTO_MQTT_RECONNECT_INITIAL_MS;
    }
    client->reconnect_at_ms = now_ms + delay;
    if (delay < PROTO_MQTT_RECONNECT_MAX_MS) {
        if (delay > (PROTO_MQTT_RECONNECT_MAX_MS / 2U)) {
            delay = PROTO_MQTT_RECONNECT_MAX_MS;
        } else {
            delay *= 2U;
        }
    }
    client->reconnect_backoff_ms = delay;
}

static void proto_mqtt_mark_disconnected(proto_mqtt_client_t *client,
                                          uint32_t now_ms)
{
    if ((client->net != NULL) && (client->net->ops != NULL) &&
        (client->net->ops->close != NULL)) {
        (void)client->net->ops->close(client->net->ctx);
    }
    client->connected = 0U;
    client->state = PROTO_MQTT_STATE_DISCONNECTED;
    client->awaiting_ping_response = 0U;
    client->control_length = 0U;
    client->control_sent = 0U;
    client->receive_length = 0U;
    if (client->queue_count > 0U) {
        client->slots[client->queue_head].sent = 0U;
    }
    proto_mqtt_schedule_reconnect(client, now_ms);
}

static sns_status_t proto_mqtt_attempt_connect(proto_mqtt_client_t *client,
                                                uint32_t now_ms,
                                                uint32_t timeout_ms)
{
    sns_status_t status;

    status = client->net->ops->connect(client->net->ctx, client->host,
                                        client->port, timeout_ms);
    if (status != SNS_OK) {
        client->connected = 0U;
        proto_mqtt_schedule_reconnect(client, now_ms);
        return status;
    }

    status = proto_mqtt_encode_connect(client);
    if (status != SNS_OK) {
        (void)client->net->ops->close(client->net->ctx);
        client->connected = 0U;
        proto_mqtt_schedule_reconnect(client, now_ms);
        return status;
    }

    client->connected = 0U;
    client->state = PROTO_MQTT_STATE_SENDING_CONNECT;
    client->awaiting_ping_response = 0U;
    client->receive_length = 0U;
    client->last_io_ms = now_ms;
    return SNS_OK;
}

static sns_status_t proto_mqtt_receive_append(proto_mqtt_client_t *client)
{
    uint16_t received = 0U;
    uint16_t available;
    sns_status_t status;

    if (client->receive_length >= client->work_capacity) {
        return SNS_ERR_NO_SPACE;
    }
    available = (uint16_t)(client->work_capacity - client->receive_length);
    status = client->net->ops->recv(client->net->ctx,
                                    &client->work_buffer[client->receive_length],
                                    available, &received);
    if (status != SNS_OK) {
        return status;
    }
    if ((received == 0U) || (received > available)) {
        return SNS_ERR_IO;
    }
    client->receive_length = (uint16_t)(client->receive_length + received);
    return SNS_OK;
}

static void proto_mqtt_receive_consume(proto_mqtt_client_t *client,
                                        uint16_t length)
{
    uint16_t remaining = (uint16_t)(client->receive_length - length);

    if (remaining > 0U) {
        (void)memmove(client->work_buffer, &client->work_buffer[length],
                      remaining);
    }
    client->receive_length = remaining;
}

static sns_status_t proto_mqtt_process_connected_receive(
    proto_mqtt_client_t *client,
    uint32_t now_ms)
{
    while (client->receive_length >= 2U) {
        if ((client->work_buffer[0] != UINT8_C(0xD0)) ||
            (client->work_buffer[1] != 0U)) {
            proto_mqtt_mark_disconnected(client, now_ms);
            return SNS_ERR_INVALID_DATA;
        }
        proto_mqtt_receive_consume(client, 2U);
        client->awaiting_ping_response = 0U;
        client->last_io_ms = now_ms;
    }

    if ((client->receive_length == 1U) &&
        (client->work_buffer[0] != UINT8_C(0xD0))) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_INVALID_DATA;
    }
    return SNS_OK;
}

static sns_status_t proto_mqtt_process_connack(proto_mqtt_client_t *client,
                                                uint32_t now_ms)
{
    uint8_t acknowledge_flags;
    uint8_t return_code;

    if ((client->receive_length >= 1U) &&
        (client->work_buffer[0] != UINT8_C(0x20))) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_INVALID_DATA;
    }
    if ((client->receive_length >= 2U) &&
        (client->work_buffer[1] != UINT8_C(0x02))) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_INVALID_DATA;
    }
    if (client->receive_length < 4U) {
        return SNS_OK;
    }

    acknowledge_flags = client->work_buffer[2];
    return_code = client->work_buffer[3];
    if ((acknowledge_flags != 0U) ||
        (return_code > UINT8_C(0x05))) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_INVALID_DATA;
    }

    proto_mqtt_receive_consume(client, 4U);
    if (return_code != 0U) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_IO;
    }

    client->state = PROTO_MQTT_STATE_CONNECTED;
    client->connected = 1U;
    client->last_io_ms = now_ms;
    client->reconnect_backoff_ms = PROTO_MQTT_RECONNECT_INITIAL_MS;
    return proto_mqtt_process_connected_receive(client, now_ms);
}

static sns_status_t proto_mqtt_send_bytes(proto_mqtt_client_t *client,
                                           const uint8_t *data,
                                           uint16_t length,
                                           uint16_t *offset,
                                           uint32_t now_ms)
{
    uint16_t sent = 0U;
    sns_status_t status;

    status = client->net->ops->send(client->net->ctx, &data[*offset],
                                     (uint16_t)(length - *offset), &sent);
    if (status != SNS_OK) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return status;
    }
    if ((sent == 0U) || (sent > (uint16_t)(length - *offset))) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_IO;
    }
    *offset = (uint16_t)(*offset + sent);
    client->last_io_ms = now_ms;
    return SNS_OK;
}

sns_status_t proto_mqtt_init(proto_mqtt_client_t *client,
                             hal_net_t *net,
                             proto_mqtt_packet_slot_t *slots,
                             uint16_t slot_count,
                             uint8_t *work_buffer,
                             uint16_t work_capacity)
{
    uint16_t index;

    if ((client == NULL) || (net == NULL) || (net->ops == NULL) ||
        (net->ops->connect == NULL) || (net->ops->send == NULL) ||
        (net->ops->recv == NULL) || (net->ops->close == NULL) ||
        (slots == NULL) || (slot_count == 0U) || (work_buffer == NULL) ||
        (work_capacity < 16U)) {
        return SNS_ERR_PARAM;
    }

    (void)memset(client, 0, sizeof(*client));
    for (index = 0U; index < slot_count; index++) {
        slots[index].length = 0U;
        slots[index].sent = 0U;
    }
    client->net = net;
    client->slots = slots;
    client->slot_count = slot_count;
    client->work_buffer = work_buffer;
    client->work_capacity = work_capacity;
    client->reconnect_backoff_ms = PROTO_MQTT_RECONNECT_INITIAL_MS;
    return SNS_OK;
}

sns_status_t proto_mqtt_connect(proto_mqtt_client_t *client,
                                const char *host,
                                uint16_t port,
                                const char *client_id,
                                uint16_t keepalive_seconds,
                                uint32_t timeout_ms,
                                uint32_t now_ms)
{
    sns_status_t status;

    if ((client == NULL) || (client->net == NULL) || (port == 0U) ||
        (keepalive_seconds == 0U) || (timeout_ms == 0U)) {
        return SNS_ERR_PARAM;
    }
    status = proto_mqtt_validate_text(host, PROTO_MQTT_HOST_CAPACITY);
    if (status != SNS_OK) {
        return status;
    }
    status = proto_mqtt_validate_text(client_id,
                                      PROTO_MQTT_CLIENT_ID_CAPACITY);
    if (status != SNS_OK) {
        return status;
    }

    if (client->state != PROTO_MQTT_STATE_DISCONNECTED) {
        (void)client->net->ops->close(client->net->ctx);
        client->state = PROTO_MQTT_STATE_DISCONNECTED;
        client->connected = 0U;
        client->control_length = 0U;
        client->control_sent = 0U;
        client->receive_length = 0U;
        if (client->queue_count > 0U) {
            client->slots[client->queue_head].sent = 0U;
        }
    }
    (void)proto_mqtt_copy_text(client->host, PROTO_MQTT_HOST_CAPACITY, host);
    (void)proto_mqtt_copy_text(client->client_id,
                               PROTO_MQTT_CLIENT_ID_CAPACITY, client_id);

    client->port = port;
    client->keepalive_seconds = keepalive_seconds;
    client->connect_timeout_ms = timeout_ms;
    client->configured = 1U;
    client->reconnect_backoff_ms = PROTO_MQTT_RECONNECT_INITIAL_MS;
    return proto_mqtt_attempt_connect(client, now_ms, timeout_ms);
}

sns_status_t proto_mqtt_publish_enqueue(proto_mqtt_client_t *client,
                                        const char *topic,
                                        const uint8_t *payload,
                                        uint16_t payload_length)
{
    proto_mqtt_packet_slot_t *slot;
    size_t topic_length;
    uint32_t remaining_length;
    uint16_t remaining_bytes = 0U;
    uint16_t offset = 1U;
    sns_status_t status;

    if ((client == NULL) || (topic == NULL) ||
        ((payload_length > 0U) && (payload == NULL))) {
        return SNS_ERR_PARAM;
    }
    topic_length = strlen(topic);
    if ((topic_length == 0U) || (topic_length > UINT16_MAX)) {
        return SNS_ERR_PARAM;
    }
    if (client->queue_count >= client->slot_count) {
        client->dropped++;
        return SNS_ERR_NO_SPACE;
    }

    remaining_length = (uint32_t)topic_length + (uint32_t)payload_length + 2U;
    slot = &client->slots[client->queue_tail];
    status = proto_mqtt_encode_remaining(remaining_length, &slot->data[offset],
                                          (uint16_t)(PROTO_MQTT_PACKET_CAPACITY - offset),
                                          &remaining_bytes);
    if (status != SNS_OK) {
        return status;
    }
    offset = (uint16_t)(offset + remaining_bytes);
    if ((uint32_t)offset + remaining_length > PROTO_MQTT_PACKET_CAPACITY) {
        return SNS_ERR_NO_SPACE;
    }

    slot->data[0] = UINT8_C(0x30);
    slot->data[offset++] = (uint8_t)(topic_length >> 8U);
    slot->data[offset++] = (uint8_t)topic_length;
    (void)memcpy(&slot->data[offset], topic, topic_length);
    offset = (uint16_t)(offset + (uint16_t)topic_length);
    if (payload_length > 0U) {
        (void)memcpy(&slot->data[offset], payload, payload_length);
        offset = (uint16_t)(offset + payload_length);
    }
    slot->length = offset;
    slot->sent = 0U;
    client->queue_tail = (uint16_t)((client->queue_tail + 1U) % client->slot_count);
    client->queue_count++;
    return SNS_OK;
}

sns_status_t proto_mqtt_poll(proto_mqtt_client_t *client,
                             uint32_t now_ms,
                             uint32_t budget_ms)
{
    uint32_t keepalive_ms;
    uint32_t attempt_timeout_ms;
    sns_status_t status;

    if ((client == NULL) || (client->net == NULL) || (budget_ms == 0U)) {
        return SNS_ERR_PARAM;
    }
    if (client->configured == 0U) {
        return SNS_ERR_STATE;
    }
    if (client->state == PROTO_MQTT_STATE_DISCONNECTED) {
        if (proto_mqtt_time_reached(now_ms, client->reconnect_at_ms) == 0U) {
            return SNS_ERR_NOT_READY;
        }
        attempt_timeout_ms = client->connect_timeout_ms;
        if (attempt_timeout_ms > budget_ms) {
            attempt_timeout_ms = budget_ms;
        }
        return proto_mqtt_attempt_connect(client, now_ms,
                                           attempt_timeout_ms);
    }

    if (client->state == PROTO_MQTT_STATE_SENDING_CONNECT) {
        status = proto_mqtt_send_bytes(client, client->work_buffer,
                                        client->control_length,
                                        &client->control_sent, now_ms);
        if ((status == SNS_OK) &&
            (client->control_sent == client->control_length)) {
            client->control_length = 0U;
            client->control_sent = 0U;
            client->receive_length = 0U;
            client->connack_deadline_ms = now_ms +
                                           client->connect_timeout_ms;
            client->state = PROTO_MQTT_STATE_WAITING_CONNACK;
        }
        return status;
    }

    if (client->state == PROTO_MQTT_STATE_WAITING_CONNACK) {
        if (proto_mqtt_time_reached(now_ms,
                                    client->connack_deadline_ms) != 0U) {
            proto_mqtt_mark_disconnected(client, now_ms);
            return SNS_ERR_TIMEOUT;
        }
        status = proto_mqtt_receive_append(client);
        if (status == SNS_ERR_NOT_READY) {
            return status;
        }
        if (status != SNS_OK) {
            proto_mqtt_mark_disconnected(client, now_ms);
            return status;
        }
        return proto_mqtt_process_connack(client, now_ms);
    }

    if (client->state != PROTO_MQTT_STATE_CONNECTED) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_STATE;
    }

    keepalive_ms = (uint32_t)client->keepalive_seconds * 1000U;
    if ((client->awaiting_ping_response != 0U) &&
        ((uint32_t)(now_ms - client->last_io_ms) >= keepalive_ms)) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return SNS_ERR_TIMEOUT;
    }

    if (client->control_sent < client->control_length) {
        status = proto_mqtt_send_bytes(client, client->work_buffer,
                                        client->control_length,
                                        &client->control_sent, now_ms);
        if ((status == SNS_OK) && (client->control_sent == client->control_length)) {
            client->control_length = 0U;
            client->control_sent = 0U;
        }
        return status;
    }

    if ((client->receive_length > 0U) ||
        (client->awaiting_ping_response != 0U)) {
        status = proto_mqtt_receive_append(client);
        if (status == SNS_OK) {
            return proto_mqtt_process_connected_receive(client, now_ms);
        }
        if (status != SNS_ERR_NOT_READY) {
            proto_mqtt_mark_disconnected(client, now_ms);
        }
        return status;
    }

    if (client->queue_count > 0U) {
        proto_mqtt_packet_slot_t *slot = &client->slots[client->queue_head];

        status = proto_mqtt_send_bytes(client, slot->data, slot->length,
                                        &slot->sent, now_ms);
        if ((status == SNS_OK) && (slot->sent == slot->length)) {
            slot->length = 0U;
            slot->sent = 0U;
            client->queue_head = (uint16_t)((client->queue_head + 1U) %
                                             client->slot_count);
            client->queue_count--;
        }
        return status;
    }

    if ((uint32_t)(now_ms - client->last_io_ms) >= keepalive_ms) {
        client->work_buffer[0] = UINT8_C(0xC0);
        client->work_buffer[1] = 0U;
        client->control_length = 2U;
        client->control_sent = 0U;
        client->awaiting_ping_response = 1U;
        status = proto_mqtt_send_bytes(client, client->work_buffer,
                                        client->control_length,
                                        &client->control_sent, now_ms);
        if ((status == SNS_OK) && (client->control_sent == client->control_length)) {
            client->control_length = 0U;
            client->control_sent = 0U;
        }
        return status;
    }

    status = proto_mqtt_receive_append(client);
    if (status == SNS_OK) {
        return proto_mqtt_process_connected_receive(client, now_ms);
    }
    if (status != SNS_ERR_NOT_READY) {
        proto_mqtt_mark_disconnected(client, now_ms);
        return status;
    }

    return SNS_ERR_NOT_READY;
}

sns_status_t proto_mqtt_close(proto_mqtt_client_t *client)
{
    sns_status_t status;

    if ((client == NULL) || (client->net == NULL) ||
        (client->net->ops == NULL) || (client->net->ops->close == NULL)) {
        return SNS_ERR_PARAM;
    }
    status = client->net->ops->close(client->net->ctx);
    client->connected = 0U;
    client->configured = 0U;
    client->state = PROTO_MQTT_STATE_DISCONNECTED;
    client->awaiting_ping_response = 0U;
    client->control_length = 0U;
    client->control_sent = 0U;
    client->receive_length = 0U;
    if (client->queue_count > 0U) {
        client->slots[client->queue_head].sent = 0U;
    }
    return status;
}

uint16_t proto_mqtt_pending(const proto_mqtt_client_t *client)
{
    return (client != NULL) ? client->queue_count : 0U;
}

uint32_t proto_mqtt_dropped(const proto_mqtt_client_t *client)
{
    return (client != NULL) ? client->dropped : 0U;
}

uint8_t proto_mqtt_is_connected(const proto_mqtt_client_t *client)
{
    return ((client != NULL) && (client->connected != 0U)) ? 1U : 0U;
}
