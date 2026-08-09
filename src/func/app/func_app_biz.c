#include "func_app_biz.h"

#include <stddef.h>
#include <string.h>

static int32_t func_app_biz_average(int64_t sum, uint16_t count)
{
    int64_t half = (int64_t)count / 2;

    if (sum < 0) {
        return (int32_t)((sum - half) / count);
    }
    return (int32_t)((sum + half) / count);
}

static void func_app_biz_refresh(func_app_biz_t *app,
                                  const func_sensor_event_t *event)
{
    uint16_t index;
    int32_t minimum = app->window[0];
    int32_t maximum = app->window[0];

    for (index = 1U; index < app->window_count; index++) {
        if (app->window[index] < minimum) {
            minimum = app->window[index];
        }
        if (app->window[index] > maximum) {
            maximum = app->window[index];
        }
    }
    app->latest.sensor_id = event->sensor_id;
    app->latest.minimum = minimum;
    app->latest.maximum = maximum;
    app->latest.average = func_app_biz_average(app->sum, app->window_count);
    app->latest.sample_count = app->window_count;
    app->latest.quality = event->quality;
    app->latest.timestamp_ms = event->timestamp_ms;
    if ((app->latest.high_alarm == 0U) &&
        (event->value >= app->high_alarm_on)) {
        app->latest.high_alarm = 1U;
    } else if ((app->latest.high_alarm != 0U) &&
               (event->value <= app->high_alarm_off)) {
        app->latest.high_alarm = 0U;
    }
    app->has_latest = 1U;
}

sns_status_t func_app_biz_init(func_app_biz_t *app,
                               func_event_queue_t *queue,
                               int32_t *window_storage,
                               uint16_t window_capacity,
                               int32_t high_alarm_on,
                               int32_t high_alarm_off,
                               uint16_t max_events_per_poll)
{
    uint16_t ignored;

    if ((app == NULL) || (window_storage == NULL) || (window_capacity == 0U) ||
        (max_events_per_poll == 0U) || (high_alarm_off > high_alarm_on) ||
        (func_event_queue_count(queue, &ignored) != SNS_OK)) {
        return SNS_ERR_PARAM;
    }
    (void)memset(app, 0, sizeof(*app));
    app->queue = queue;
    app->window = window_storage;
    app->window_capacity = window_capacity;
    app->high_alarm_on = high_alarm_on;
    app->high_alarm_off = high_alarm_off;
    app->max_events_per_poll = max_events_per_poll;
    return SNS_OK;
}

sns_status_t func_app_biz_poll(func_app_biz_t *app, uint32_t now_ms)
{
    uint16_t index;
    func_sensor_event_t event;
    sns_status_t status;

    (void)now_ms;
    if ((app == NULL) || (app->queue == NULL) || (app->window == NULL)) {
        return SNS_ERR_PARAM;
    }
    for (index = 0U; index < app->max_events_per_poll; index++) {
        status = func_event_queue_pop(app->queue, &event);
        if (status == SNS_ERR_NOT_FOUND) {
            return SNS_OK;
        }
        if (status != SNS_OK) {
            return status;
        }
        if (event.quality != FUNC_QUALITY_VALID) {
            app->latest.sensor_id = event.sensor_id;
            app->latest.quality = event.quality;
            app->latest.timestamp_ms = event.timestamp_ms;
            app->has_latest = 1U;
            continue;
        }
        if (app->window_count == app->window_capacity) {
            app->sum -= app->window[app->window_next];
        } else {
            app->window_count++;
        }
        app->window[app->window_next] = event.value;
        app->sum += event.value;
        app->window_next = (uint16_t)((app->window_next + 1U) % app->window_capacity);
        func_app_biz_refresh(app, &event);
    }
    return SNS_OK;
}

sns_status_t func_app_biz_get_latest(const func_app_biz_t *app,
                                     func_biz_result_t *result)
{
    if ((app == NULL) || (result == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (app->has_latest == 0U) {
        return SNS_ERR_NOT_READY;
    }
    *result = app->latest;
    return SNS_OK;
}
