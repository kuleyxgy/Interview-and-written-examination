#ifndef FUNC_APP_RUNTIME_H
#define FUNC_APP_RUNTIME_H

#include <stdint.h>

#include "func_app_biz.h"
#include "func_app_gui.h"
#include "func_app_mqtt.h"
#include "proto_clock.h"

typedef struct {
    proto_clock_t *clock;
    func_app_gui_t *gui;
    func_app_biz_t *biz;
    func_app_mqtt_t *mqtt_app;
    proto_mqtt_client_t *mqtt_client;
    uint32_t mqtt_poll_budget_ms;
} func_app_runtime_t;

sns_status_t func_app_runtime_init(func_app_runtime_t *runtime,
                                   proto_clock_t *clock,
                                   func_app_gui_t *gui,
                                   func_app_biz_t *biz,
                                   func_app_mqtt_t *mqtt_app,
                                   proto_mqtt_client_t *mqtt_client,
                                   uint32_t mqtt_poll_budget_ms);
sns_status_t func_app_runtime_poll_once(func_app_runtime_t *runtime,
                                        uint32_t *now_ms);

#endif
