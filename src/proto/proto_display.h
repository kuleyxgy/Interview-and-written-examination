#ifndef PROTO_DISPLAY_H
#define PROTO_DISPLAY_H

#include <stdint.h>

#include "util_status.h"

#define PROTO_DISPLAY_VALUE_TEXT_CAPACITY   24U
#define PROTO_DISPLAY_UNIT_TEXT_CAPACITY     8U
#define PROTO_DISPLAY_QUALITY_TEXT_CAPACITY 12U

typedef struct {
    uint16_t sensor_id;
    char value_text[PROTO_DISPLAY_VALUE_TEXT_CAPACITY];
    char unit_text[PROTO_DISPLAY_UNIT_TEXT_CAPACITY];
    char quality_text[PROTO_DISPLAY_QUALITY_TEXT_CAPACITY];
    uint32_t timestamp_ms;
} proto_display_record_t;

typedef struct proto_display proto_display_t;

typedef struct {
    sns_status_t (*show)(void *ctx, const proto_display_record_t *record);
} proto_display_ops_t;

struct proto_display {
    const proto_display_ops_t *ops;
    void *ctx;
};

sns_status_t proto_display_show(proto_display_t *display,
                                const proto_display_record_t *record);

#endif
