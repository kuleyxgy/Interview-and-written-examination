#ifndef FUNC_SENSOR_H
#define FUNC_SENSOR_H

#include <stdint.h>

#include "func_cfg.h"
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

typedef struct {
    func_sensor_registration_t registration;
    func_sensor_event_t latest;
    uint32_t next_sequence;
    uint8_t has_latest;
} func_sensor_slot_t;

typedef struct {
    func_sensor_id_t sensor_id;
    func_event_queue_t *queue;
} func_sensor_subscription_t;

typedef struct {
    func_sensor_slot_t slots[FUNC_CFG_MAX_SENSORS];
    func_sensor_subscription_t subscriptions[FUNC_CFG_MAX_SUBSCRIPTIONS];
    uint16_t sensor_count;
    uint16_t subscription_count;
    uint8_t initialized;
} func_sensor_core_t;

sns_status_t func_sensor_core_init(func_sensor_core_t *core);
sns_status_t func_sensor_reset(func_sensor_core_t *core);
sns_status_t func_sensor_register(
    func_sensor_core_t *core,
    const func_sensor_registration_t *registration);
sns_status_t func_sensor_subscribe(func_sensor_core_t *core,
                                   func_sensor_id_t sensor_id,
                                   func_event_queue_t *queue);
sns_status_t func_sensor_poll_all(func_sensor_core_t *core,
                                  uint32_t now_ms);
sns_status_t func_sensor_get_latest(func_sensor_core_t *core,
                                    func_sensor_id_t id,
                                    func_sensor_event_t *snapshot);

#endif
