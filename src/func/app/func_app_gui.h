#ifndef FUNC_APP_GUI_H
#define FUNC_APP_GUI_H

#include <stdint.h>

#include "func_event_queue.h"
#include "proto_display.h"

typedef struct {
    func_event_queue_t *queue;
    proto_display_t *display;
    uint16_t max_events_per_poll;
} func_app_gui_t;

sns_status_t func_app_gui_init(func_app_gui_t *app,
                               func_event_queue_t *queue,
                               proto_display_t *display,
                               uint16_t max_events_per_poll);
sns_status_t func_app_gui_poll(func_app_gui_t *app, uint32_t now_ms);

#endif
