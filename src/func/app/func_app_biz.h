#ifndef FUNC_APP_BIZ_H
#define FUNC_APP_BIZ_H

#include <stdint.h>

#include "func_event_queue.h"

typedef struct {
    func_sensor_id_t sensor_id;
    int32_t minimum;
    int32_t maximum;
    int32_t average;
    uint16_t sample_count;
    uint8_t high_alarm;
    func_quality_t quality;
    uint32_t timestamp_ms;
} func_biz_result_t;

typedef struct {
    func_event_queue_t *queue;
    func_sensor_id_t sensor_id;
    int32_t *window;
    uint16_t window_capacity;
    uint16_t window_count;
    uint16_t window_next;
    uint16_t max_events_per_poll;
    int64_t sum;
    int32_t high_alarm_on;
    int32_t high_alarm_off;
    func_biz_result_t latest;
    uint8_t has_latest;
} func_app_biz_t;

sns_status_t func_app_biz_init(func_app_biz_t *app,
                               func_event_queue_t *queue,
                               func_sensor_id_t sensor_id,
                               int32_t *window_storage,
                               uint16_t window_capacity,
                               int32_t high_alarm_on,
                               int32_t high_alarm_off,
                               uint16_t max_events_per_poll);
sns_status_t func_app_biz_poll(func_app_biz_t *app, uint32_t now_ms);
sns_status_t func_app_biz_get_latest(const func_app_biz_t *app,
                                     func_biz_result_t *result);

#endif
