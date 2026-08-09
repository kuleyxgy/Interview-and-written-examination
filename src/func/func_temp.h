#ifndef FUNC_TEMP_H
#define FUNC_TEMP_H

#include <stdint.h>

#include "func_sensor.h"
#include "filter/func_filter.h"
#include "proto_temp.h"

#define FUNC_TEMP_MIN_MDEG_C (-55000)
#define FUNC_TEMP_MAX_MDEG_C 125000

typedef struct {
    proto_temp_device_t *source;
    uint32_t sample_period_ms;
    int32_t calibration_gain_ppm;
    int32_t calibration_offset_mdeg_c;
    func_filter_chain_t filters;
    int32_t publish_change_mdeg_c;
    uint32_t force_publish_period_ms;
    uint16_t error_threshold;
} func_temp_cfg_t;

typedef struct {
    func_temp_cfg_t cfg;
    uint32_t last_sample_ms;
    uint32_t last_valid_publish_ms;
    int32_t last_valid_value;
    int32_t last_published_value;
    uint16_t consecutive_errors;
    func_quality_t last_quality;
    uint8_t initialized;
    uint8_t sample_started;
    uint8_t has_valid;
    uint8_t has_valid_publish;
    uint8_t has_published_quality;
} func_temp_t;

sns_status_t func_temp_configure(func_temp_t *temp,
                                 const func_temp_cfg_t *cfg);
sns_status_t func_temp_init(void *ctx);
sns_status_t func_temp_poll(void *ctx,
                            uint32_t now_ms,
                            func_sensor_event_t *event,
                            uint8_t *event_ready);
sns_status_t func_temp_deinit(void *ctx);

extern const func_sensor_driver_ops_t func_temp_driver_ops;

#endif
