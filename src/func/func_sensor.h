#ifndef FUNC_SENSOR_H
#define FUNC_SENSOR_H

#include <stdint.h>

#include "func_event_queue.h"
#include "func_sensor_types.h"

typedef struct {
    sns_status_t (*init)(void *ctx);
    sns_status_t (*poll)(void *ctx,
                         uint32_t now_ms,
                         func_sensor_event_t *event,
                         uint8_t *event_ready);
    sns_status_t (*deinit)(void *ctx);
} func_sensor_driver_ops_t;

typedef struct {
    func_sensor_id_t id;
    const char *name;
    const func_sensor_driver_ops_t *ops;
    void *driver_ctx;
} func_sensor_registration_t;

sns_status_t func_sensor_reset(void);
sns_status_t func_sensor_register(
    const func_sensor_registration_t *registration);
sns_status_t func_sensor_subscribe(func_sensor_id_t sensor_id,
                                   func_event_queue_t *queue);
sns_status_t func_sensor_poll_all(uint32_t now_ms);
sns_status_t func_sensor_get_latest(func_sensor_id_t id,
                                    func_sensor_event_t *snapshot);

#endif
