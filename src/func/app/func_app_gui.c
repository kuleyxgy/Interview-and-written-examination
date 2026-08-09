#include "func_app_gui.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *func_app_gui_quality(func_quality_t quality)
{
    switch (quality) {
    case FUNC_QUALITY_VALID:
        return "valid";
    case FUNC_QUALITY_STALE:
        return "stale";
    case FUNC_QUALITY_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

static sns_status_t func_app_gui_record(const func_sensor_event_t *event,
                                         proto_display_record_t *record)
{
    int32_t whole;
    int32_t fraction;
    int written;

    if ((event == NULL) || (record == NULL)) {
        return SNS_ERR_PARAM;
    }
    if ((event->kind != FUNC_MEAS_TEMPERATURE) ||
        (event->unit != FUNC_UNIT_MDEG_C)) {
        return SNS_ERR_UNSUPPORTED;
    }

    (void)memset(record, 0, sizeof(*record));
    whole = event->value / 1000;
    fraction = event->value % 1000;
    if (fraction < 0) {
        fraction = -fraction;
    }
    if ((event->value < 0) && (whole == 0)) {
        written = snprintf(record->value_text, sizeof(record->value_text),
                           "-0.%03ld", (long)fraction);
    } else {
        written = snprintf(record->value_text, sizeof(record->value_text),
                           "%ld.%03ld", (long)whole, (long)fraction);
    }
    if ((written < 0) || ((size_t)written >= sizeof(record->value_text))) {
        return SNS_ERR_NO_SPACE;
    }
    (void)memcpy(record->unit_text, "C", 2U);
    (void)snprintf(record->quality_text, sizeof(record->quality_text), "%s",
                   func_app_gui_quality(event->quality));
    record->sensor_id = event->sensor_id;
    record->timestamp_ms = event->timestamp_ms;
    return SNS_OK;
}

sns_status_t func_app_gui_init(func_app_gui_t *app,
                               func_event_queue_t *queue,
                               proto_display_t *display,
                               uint16_t max_events_per_poll)
{
    uint16_t ignored;

    if ((app == NULL) || (display == NULL) || (display->ops == NULL) ||
        (display->ops->show == NULL) || (max_events_per_poll == 0U) ||
        (func_event_queue_count(queue, &ignored) != SNS_OK)) {
        return SNS_ERR_PARAM;
    }
    app->queue = queue;
    app->display = display;
    app->max_events_per_poll = max_events_per_poll;
    return SNS_OK;
}

sns_status_t func_app_gui_poll(func_app_gui_t *app, uint32_t now_ms)
{
    uint16_t index;
    func_sensor_event_t event;
    proto_display_record_t record;
    sns_status_t status;

    (void)now_ms;
    if ((app == NULL) || (app->queue == NULL) || (app->display == NULL)) {
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
        status = func_app_gui_record(&event, &record);
        if (status != SNS_OK) {
            return status;
        }
        status = proto_display_show(app->display, &record);
        if (status != SNS_OK) {
            return status;
        }
    }
    return SNS_OK;
}
