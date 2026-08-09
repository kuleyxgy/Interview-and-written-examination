#ifndef FUNC_SENSOR_TYPES_H
#define FUNC_SENSOR_TYPES_H

#include <stdint.h>

#include "util_status.h"

typedef uint16_t func_sensor_id_t;

typedef enum {
    FUNC_MEAS_TEMPERATURE = 0
} func_measurement_kind_t;

typedef enum {
    FUNC_UNIT_MDEG_C = 0
} func_measurement_unit_t;

typedef enum {
    FUNC_QUALITY_VALID = 0,
    FUNC_QUALITY_STALE,
    FUNC_QUALITY_ERROR
} func_quality_t;

typedef struct {
    func_sensor_id_t sensor_id;
    func_measurement_kind_t kind;
    func_measurement_unit_t unit;
    int32_t value;
    uint32_t timestamp_ms;
    uint32_t sequence;
    func_quality_t quality;
    sns_status_t status;
} func_sensor_event_t;

#endif
