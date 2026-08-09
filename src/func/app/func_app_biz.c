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

static func_biz_sensor_state_t *func_app_biz_find_state(
    const func_app_biz_t *app,
    func_sensor_id_t sensor_id)
{
    uint16_t index;

    for (index = 0U; index < app->state_count; index++) {
        if (app->states[index].sensor_id == sensor_id) {
            return &app->states[index];
        }
    }
    return NULL;
}

static void func_app_biz_refresh(func_biz_sensor_state_t *state,
                                 const func_sensor_event_t *event)
{
    uint16_t index;
    int32_t minimum = state->window[0];
    int32_t maximum = state->window[0];

    for (index = 1U; index < state->window_count; index++) {
        if (state->window[index] < minimum) {
            minimum = state->window[index];
        }
        if (state->window[index] > maximum) {
            maximum = state->window[index];
        }
    }
    state->latest.sensor_id = event->sensor_id;
    state->latest.minimum = minimum;
    state->latest.maximum = maximum;
    state->latest.average = func_app_biz_average(state->sum,
                                                  state->window_count);
    state->latest.sample_count = state->window_count;
    state->latest.quality = event->quality;
    state->latest.timestamp_ms = event->timestamp_ms;
    if ((state->latest.high_alarm == 0U) &&
        (event->value >= state->high_alarm_on)) {
        state->latest.high_alarm = 1U;
    } else if ((state->latest.high_alarm != 0U) &&
               (event->value <= state->high_alarm_off)) {
        state->latest.high_alarm = 0U;
    }
    state->has_latest = 1U;
}

sns_status_t func_biz_sensor_state_init(func_biz_sensor_state_t *state,
                                        func_sensor_id_t sensor_id,
                                        int32_t *window_storage,
                                        uint16_t window_capacity,
                                        int32_t high_alarm_on,
                                        int32_t high_alarm_off)
{
    if ((state == NULL) || (window_storage == NULL) ||
        (window_capacity == 0U) || (high_alarm_off > high_alarm_on)) {
        return SNS_ERR_PARAM;
    }
    (void)memset(state, 0, sizeof(*state));
    state->sensor_id = sensor_id;
    state->window = window_storage;
    state->window_capacity = window_capacity;
    state->high_alarm_on = high_alarm_on;
    state->high_alarm_off = high_alarm_off;
    state->initialized = 1U;
    return SNS_OK;
}

sns_status_t func_app_biz_init(func_app_biz_t *app,
                               func_event_queue_t *queue,
                               func_biz_sensor_state_t *states,
                               uint16_t state_count,
                               uint16_t max_events_per_poll)
{
    uint16_t index;
    uint16_t other;
    uint16_t ignored;

    if ((app == NULL) || (states == NULL) || (state_count == 0U) ||
        (max_events_per_poll == 0U) ||
        (func_event_queue_count(queue, &ignored) != SNS_OK)) {
        return SNS_ERR_PARAM;
    }
    for (index = 0U; index < state_count; index++) {
        if ((states[index].initialized == 0U) ||
            (states[index].window == NULL) ||
            (states[index].window_capacity == 0U)) {
            return SNS_ERR_PARAM;
        }
        for (other = (uint16_t)(index + 1U); other < state_count; other++) {
            if (states[index].sensor_id == states[other].sensor_id) {
                return SNS_ERR_STATE;
            }
        }
    }
    app->queue = queue;
    app->states = states;
    app->state_count = state_count;
    app->max_events_per_poll = max_events_per_poll;
    return SNS_OK;
}

sns_status_t func_app_biz_poll(func_app_biz_t *app, uint32_t now_ms)
{
    uint16_t index;
    func_biz_sensor_state_t *state;
    func_sensor_event_t event;
    sns_status_t status;

    (void)now_ms;
    if ((app == NULL) || (app->queue == NULL) || (app->states == NULL) ||
        (app->state_count == 0U)) {
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
        state = func_app_biz_find_state(app, event.sensor_id);
        if (state == NULL) {
            continue;
        }
        if (event.quality != FUNC_QUALITY_VALID) {
            state->latest.sensor_id = event.sensor_id;
            state->latest.quality = event.quality;
            state->latest.timestamp_ms = event.timestamp_ms;
            state->has_latest = 1U;
            continue;
        }
        if (state->window_count == state->window_capacity) {
            state->sum -= state->window[state->window_next];
        } else {
            state->window_count++;
        }
        state->window[state->window_next] = event.value;
        state->sum += event.value;
        state->window_next = (uint16_t)((state->window_next + 1U) %
                                        state->window_capacity);
        func_app_biz_refresh(state, &event);
    }
    return SNS_OK;
}

sns_status_t func_app_biz_get_latest(const func_app_biz_t *app,
                                     func_sensor_id_t sensor_id,
                                     func_biz_result_t *result)
{
    func_biz_sensor_state_t *state;

    if ((app == NULL) || (app->states == NULL) || (result == NULL)) {
        return SNS_ERR_PARAM;
    }
    state = func_app_biz_find_state(app, sensor_id);
    if (state == NULL) {
        return SNS_ERR_NOT_FOUND;
    }
    if (state->has_latest == 0U) {
        return SNS_ERR_NOT_READY;
    }
    *result = state->latest;
    return SNS_OK;
}
