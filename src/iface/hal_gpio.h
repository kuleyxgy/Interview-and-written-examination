#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

#include "util_status.h"

typedef struct hal_gpio hal_gpio_t;

typedef enum {
    HAL_GPIO_MODE_INPUT = 0,
    HAL_GPIO_MODE_OUTPUT,
    HAL_GPIO_MODE_INPUT_PULL_UP,
    HAL_GPIO_MODE_INPUT_PULL_DOWN
} hal_gpio_mode_t;

typedef enum {
    HAL_GPIO_LOW = 0,
    HAL_GPIO_HIGH = 1
} hal_gpio_level_t;

typedef struct {
    sns_status_t (*configure)(void *ctx, uint16_t pin, hal_gpio_mode_t mode);
    sns_status_t (*write)(void *ctx, uint16_t pin, hal_gpio_level_t level);
    sns_status_t (*read)(void *ctx, uint16_t pin, hal_gpio_level_t *level);
} hal_gpio_ops_t;

struct hal_gpio {
    const hal_gpio_ops_t *ops;
    void *ctx;
};

#endif
