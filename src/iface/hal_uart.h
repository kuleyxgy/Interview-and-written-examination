#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

#include "util_status.h"

typedef struct hal_uart hal_uart_t;

typedef struct {
    sns_status_t (*send)(void *ctx,
                         const uint8_t *data,
                         uint16_t length,
                         uint32_t timeout_ms,
                         uint16_t *sent);
    sns_status_t (*recv)(void *ctx,
                         uint8_t *data,
                         uint16_t capacity,
                         uint32_t timeout_ms,
                         uint16_t *received);
} hal_uart_ops_t;

struct hal_uart {
    const hal_uart_ops_t *ops;
    void *ctx;
};

#endif
