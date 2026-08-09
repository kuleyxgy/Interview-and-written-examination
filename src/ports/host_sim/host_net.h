#ifndef HOST_NET_H
#define HOST_NET_H

#include <stdint.h>

#include "hal_cfg.h"
#include "hal_net.h"
#include "util_status.h"

typedef struct {
    uint8_t data[HAL_CFG_HOST_NET_MAX_PACKET];
    uint16_t length;
} host_net_packet_t;

typedef struct {
    uint8_t connected;
    char host[HAL_CFG_HOST_NET_MAX_HOST_LENGTH + 1U];
    uint16_t port;
    host_net_packet_t captures[HAL_CFG_HOST_NET_MAX_CAPTURES];
    uint16_t capture_count;
    host_net_packet_t rx_queue[HAL_CFG_HOST_NET_RX_QUEUE_DEPTH];
    uint16_t rx_read_index;
    uint16_t rx_write_index;
    uint16_t rx_count;
} host_net_t;

sns_status_t host_net_init(host_net_t *host);
sns_status_t host_net_bind(host_net_t *host, hal_net_t *hal);
sns_status_t host_net_receive_push(host_net_t *host,
                                   const uint8_t *data,
                                   uint16_t length);
sns_status_t host_net_capture_at(const host_net_t *host,
                                 uint16_t index,
                                 const uint8_t **data,
                                 uint16_t *length);
sns_status_t host_net_reset_captures(host_net_t *host);
uint16_t host_net_capture_count(const host_net_t *host);
uint8_t host_net_is_connected(const host_net_t *host);

#endif
