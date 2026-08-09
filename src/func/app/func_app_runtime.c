#include "func_app_runtime.h"

#include <stddef.h>

#include "func_sensor.h"

static void func_app_runtime_capture(sns_status_t status,
                                     sns_status_t *first_error)
{
    if ((status != SNS_OK) && (status != SNS_ERR_NOT_READY) &&
        (*first_error == SNS_OK)) {
        *first_error = status;
    }
}

sns_status_t func_app_runtime_init(func_app_runtime_t *runtime,
                                   proto_clock_t *clock,
                                   func_app_gui_t *gui,
                                   func_app_biz_t *biz,
                                   func_app_mqtt_t *mqtt_app,
                                   proto_mqtt_client_t *mqtt_client,
                                   uint32_t mqtt_poll_budget_ms)
{
    if ((runtime == NULL) || (clock == NULL) || (gui == NULL) ||
        (biz == NULL) || (mqtt_app == NULL) || (mqtt_client == NULL) ||
        (mqtt_poll_budget_ms == 0U)) {
        return SNS_ERR_PARAM;
    }
    runtime->clock = clock;
    runtime->gui = gui;
    runtime->biz = biz;
    runtime->mqtt_app = mqtt_app;
    runtime->mqtt_client = mqtt_client;
    runtime->mqtt_poll_budget_ms = mqtt_poll_budget_ms;
    return SNS_OK;
}

sns_status_t func_app_runtime_poll_once(func_app_runtime_t *runtime,
                                        uint32_t *now_ms)
{
    uint32_t current;
    sns_status_t status;
    sns_status_t first_error = SNS_OK;

    if ((runtime == NULL) || (now_ms == NULL)) {
        return SNS_ERR_PARAM;
    }
    status = proto_clock_now_ms(runtime->clock, &current);
    if (status != SNS_OK) {
        return status;
    }
    *now_ms = current;
    func_app_runtime_capture(func_sensor_poll_all(current), &first_error);
    func_app_runtime_capture(func_app_gui_poll(runtime->gui, current), &first_error);
    func_app_runtime_capture(func_app_biz_poll(runtime->biz, current), &first_error);
    func_app_runtime_capture(func_app_mqtt_poll(runtime->mqtt_app, current), &first_error);
    func_app_runtime_capture(proto_mqtt_poll(runtime->mqtt_client, current,
                                              runtime->mqtt_poll_budget_ms),
                             &first_error);
    return first_error;
}
