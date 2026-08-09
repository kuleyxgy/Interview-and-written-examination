#include "host_net.h"

#include <stddef.h>
#include <string.h>

static sns_status_t host_net_connect(void *ctx,
                                     const char *hostname,
                                     uint16_t port,
                                     uint32_t timeout_ms)
{
    host_net_t *host = (host_net_t *)ctx;
    size_t hostname_length;

    (void)timeout_ms;

    if ((host == NULL) || (hostname == NULL) || (port == 0U)) {
        return SNS_ERR_PARAM;
    }

    hostname_length = strlen(hostname);
    if (hostname_length > HAL_CFG_HOST_NET_MAX_HOST_LENGTH) {
        return SNS_ERR_NO_SPACE;
    }

    (void)memcpy(host->host, hostname, hostname_length + 1U);
    host->port = port;
    host->connected = 1U;
    return SNS_OK;
}

static sns_status_t host_net_send(void *ctx,
                                  const uint8_t *data,
                                  uint16_t length,
                                  uint16_t *sent)
{
    host_net_t *host = (host_net_t *)ctx;
    host_net_packet_t *packet;

    if ((host == NULL) || (sent == NULL) ||
        ((length > 0U) && (data == NULL))) {
        return SNS_ERR_PARAM;
    }
    if (host->connected == 0U) {
        return SNS_ERR_STATE;
    }
    if ((length > HAL_CFG_HOST_NET_MAX_PACKET) ||
        (host->capture_count >= HAL_CFG_HOST_NET_MAX_CAPTURES)) {
        return SNS_ERR_NO_SPACE;
    }

    packet = &host->captures[host->capture_count];
    if (length > 0U) {
        (void)memcpy(packet->data, data, length);
    }
    packet->length = length;
    host->capture_count++;
    *sent = length;
    return SNS_OK;
}

static sns_status_t host_net_recv(void *ctx,
                                  uint8_t *data,
                                  uint16_t capacity,
                                  uint16_t *received)
{
    host_net_t *host = (host_net_t *)ctx;
    const host_net_packet_t *packet;

    if ((host == NULL) || (received == NULL) ||
        ((capacity > 0U) && (data == NULL))) {
        return SNS_ERR_PARAM;
    }
    if (host->connected == 0U) {
        return SNS_ERR_STATE;
    }
    if (host->rx_count == 0U) {
        return SNS_ERR_NOT_READY;
    }

    packet = &host->rx_queue[host->rx_read_index];
    if (capacity < packet->length) {
        return SNS_ERR_NO_SPACE;
    }

    if (packet->length > 0U) {
        (void)memcpy(data, packet->data, packet->length);
    }
    *received = packet->length;
    host->rx_read_index = (uint16_t)((host->rx_read_index + 1U) %
                                     HAL_CFG_HOST_NET_RX_QUEUE_DEPTH);
    host->rx_count--;
    return SNS_OK;
}

static sns_status_t host_net_close(void *ctx)
{
    host_net_t *host = (host_net_t *)ctx;

    if (host == NULL) {
        return SNS_ERR_PARAM;
    }

    host->connected = 0U;
    return SNS_OK;
}

static const hal_net_ops_t host_net_ops = {
    host_net_connect,
    host_net_send,
    host_net_recv,
    host_net_close
};

sns_status_t host_net_init(host_net_t *host)
{
    if (host == NULL) {
        return SNS_ERR_PARAM;
    }

    (void)memset(host, 0, sizeof(*host));
    return SNS_OK;
}

sns_status_t host_net_bind(host_net_t *host, hal_net_t *hal)
{
    if ((host == NULL) || (hal == NULL)) {
        return SNS_ERR_PARAM;
    }

    hal->ops = &host_net_ops;
    hal->ctx = host;
    return SNS_OK;
}

sns_status_t host_net_receive_push(host_net_t *host,
                                   const uint8_t *data,
                                   uint16_t length)
{
    host_net_packet_t *packet;

    if ((host == NULL) || ((length > 0U) && (data == NULL))) {
        return SNS_ERR_PARAM;
    }
    if ((length > HAL_CFG_HOST_NET_MAX_PACKET) ||
        (host->rx_count >= HAL_CFG_HOST_NET_RX_QUEUE_DEPTH)) {
        return SNS_ERR_NO_SPACE;
    }

    packet = &host->rx_queue[host->rx_write_index];
    if (length > 0U) {
        (void)memcpy(packet->data, data, length);
    }
    packet->length = length;
    host->rx_write_index = (uint16_t)((host->rx_write_index + 1U) %
                                      HAL_CFG_HOST_NET_RX_QUEUE_DEPTH);
    host->rx_count++;
    return SNS_OK;
}

sns_status_t host_net_capture_at(const host_net_t *host,
                                 uint16_t index,
                                 const uint8_t **data,
                                 uint16_t *length)
{
    const uint8_t *captured_data;
    uint16_t captured_length;

    if ((host == NULL) || (data == NULL) || (length == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (index >= host->capture_count) {
        return SNS_ERR_NOT_FOUND;
    }

    captured_data = host->captures[index].data;
    captured_length = host->captures[index].length;
    *data = captured_data;
    *length = captured_length;
    return SNS_OK;
}

sns_status_t host_net_reset_captures(host_net_t *host)
{
    if (host == NULL) {
        return SNS_ERR_PARAM;
    }

    host->capture_count = 0U;
    return SNS_OK;
}

uint16_t host_net_capture_count(const host_net_t *host)
{
    return (host != NULL) ? host->capture_count : 0U;
}

uint8_t host_net_is_connected(const host_net_t *host)
{
    return ((host != NULL) && (host->connected != 0U)) ? 1U : 0U;
}
