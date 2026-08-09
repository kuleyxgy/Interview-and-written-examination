#ifndef FUNC_EVENT_QUEUE_H
#define FUNC_EVENT_QUEUE_H

#include <stdint.h>

#include "func_sensor_types.h"

typedef enum {
    FUNC_QUEUE_DROP_NEWEST = 0,
    FUNC_QUEUE_DROP_OLDEST
} func_queue_overflow_policy_t;

typedef struct {
    func_sensor_event_t *storage;
    uint16_t capacity;
    uint16_t read_index;
    uint16_t write_index;
    uint16_t count;
    uint32_t dropped;
    func_queue_overflow_policy_t overflow_policy;
} func_event_queue_t;

sns_status_t func_event_queue_init(
    func_event_queue_t *queue,
    func_sensor_event_t *storage,
    uint16_t capacity,
    func_queue_overflow_policy_t overflow_policy);
sns_status_t func_event_queue_push(func_event_queue_t *queue,
                                   const func_sensor_event_t *event);
sns_status_t func_event_queue_pop(func_event_queue_t *queue,
                                  func_sensor_event_t *event);
sns_status_t func_event_queue_count(const func_event_queue_t *queue,
                                    uint16_t *count);
sns_status_t func_event_queue_dropped(const func_event_queue_t *queue,
                                      uint32_t *dropped);

#endif
