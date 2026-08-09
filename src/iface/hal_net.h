#ifndef HAL_NET_H
#define HAL_NET_H

#include <stdint.h>

#include "util_status.h"

typedef struct hal_net hal_net_t;

typedef struct {
    sns_status_t (*connect)(void *ctx,
                            const char *host,
                            uint16_t port,
                            uint32_t timeout_ms);
    sns_status_t (*send)(void *ctx,
                         const uint8_t *data,
                         uint16_t length,
                         uint16_t *sent);
    sns_status_t (*recv)(void *ctx,
                         uint8_t *data,
                         uint16_t capacity,
                         uint16_t *received);
    sns_status_t (*close)(void *ctx);
} hal_net_ops_t;

struct hal_net {
    const hal_net_ops_t *ops;
    void *ctx;
};

#endif
