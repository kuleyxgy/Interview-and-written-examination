#include "func_temp.h"

#include <limits.h>
#include <stddef.h>

#include "util_math.h"
#include "func_cfg.h"

#define FUNC_TEMP_PPM_SCALE 1000000LL

static sns_status_t temp_cfg_valid(const func_temp_cfg_t *cfg)
{
    if ((cfg == NULL) || (cfg->source == NULL) ||
        (cfg->source->ops == NULL) || (cfg->source->ops->init == NULL) ||
        (cfg->source->ops->read_mdeg_c == NULL) ||
        (cfg->sample_period_ms == 0U) ||
        (cfg->publish_change_mdeg_c < 0) ||
        (cfg->calibration_gain_ppm <= -1000000)) {
        return SNS_ERR_PARAM;
    }
    return SNS_OK;
}

sns_status_t func_temp_configure(func_temp_t *temp,
                                 const func_temp_cfg_t *cfg)
{
    sns_status_t status;

    if (temp == NULL) {
        return SNS_ERR_PARAM;
    }
    status = temp_cfg_valid(cfg);
    if (status != SNS_OK) {
        return status;
    }
    temp->cfg = *cfg;
    temp->last_sample_ms = 0U;
    temp->last_valid_publish_ms = 0U;
    temp->last_valid_value = 0;
    temp->last_published_value = 0;
    temp->consecutive_errors = 0U;
    temp->last_quality = FUNC_QUALITY_ERROR;
    temp->initialized = 0U;
    temp->sample_started = 0U;
    temp->has_valid = 0U;
    temp->has_valid_publish = 0U;
    temp->has_published_quality = 0U;
    return SNS_OK;
}

sns_status_t func_temp_init(void *ctx)
{
    func_temp_t *temp;
    sns_status_t status;

    if (ctx == NULL) {
        return SNS_ERR_PARAM;
    }
    temp = (func_temp_t *)ctx;
    status = temp_cfg_valid(&temp->cfg);
    if (status != SNS_OK) {
        return SNS_ERR_STATE;
    }
    status = func_filter_chain_init(&temp->cfg.filters);
    if (status != SNS_OK) {
        return status;
    }
    status = temp->cfg.source->ops->init(temp->cfg.source->ctx);
    if (status != SNS_OK) {
        return status;
    }
    temp->last_sample_ms = 0U;
    temp->last_valid_publish_ms = 0U;
    temp->last_valid_value = 0;
    temp->last_published_value = 0;
    temp->consecutive_errors = 0U;
    temp->last_quality = FUNC_QUALITY_ERROR;
    temp->sample_started = 0U;
    temp->has_valid = 0U;
    temp->has_valid_publish = 0U;
    temp->has_published_quality = 0U;
    temp->initialized = 1U;
    return SNS_OK;
}

static void temp_event_base(func_sensor_event_t *event, uint32_t now_ms)
{
    event->sensor_id = 0U;
    event->kind = FUNC_MEAS_TEMPERATURE;
    event->unit = FUNC_UNIT_MDEG_C;
    event->timestamp_ms = now_ms;
    event->sequence = 0U;
}

static sns_status_t temp_error_event(func_temp_t *temp,
                                     uint32_t now_ms,
                                     sns_status_t cause,
                                     func_sensor_event_t *event,
                                     uint8_t *event_ready)
{
    uint16_t error_threshold;

    if (temp->consecutive_errors < UINT16_MAX) {
        ++temp->consecutive_errors;
    }
    temp_event_base(event, now_ms);
    event->value = (temp->has_valid != 0U) ? temp->last_valid_value : 0;
    error_threshold = temp->cfg.error_threshold;
    if (error_threshold == 0U) {
        error_threshold = FUNC_CFG_TEMP_ERROR_THRESHOLD;
    }
    if ((temp->has_valid == 0U) ||
        (temp->consecutive_errors >= error_threshold)) {
        event->quality = FUNC_QUALITY_ERROR;
    } else {
        event->quality = FUNC_QUALITY_STALE;
    }
    event->status = cause;
    temp->last_quality = event->quality;
    temp->has_published_quality = 1U;
    *event_ready = 1U;
    return cause;
}

static sns_status_t temp_calibrate(const func_temp_t *temp,
                                   int32_t raw,
                                   int32_t *calibrated)
{
    int64_t scaled;
    int64_t rounded;
    sns_status_t status;

    scaled = (int64_t)raw *
             (FUNC_TEMP_PPM_SCALE +
              (int64_t)temp->cfg.calibration_gain_ppm);
    status = util_div_round_nearest_i64(scaled, FUNC_TEMP_PPM_SCALE, &rounded);
    if (status != SNS_OK) {
        return status;
    }
    rounded += (int64_t)temp->cfg.calibration_offset_mdeg_c;
    if ((rounded < INT32_MIN) || (rounded > INT32_MAX)) {
        return SNS_ERR_INVALID_DATA;
    }
    *calibrated = (int32_t)rounded;
    return SNS_OK;
}

static uint8_t temp_publish_due(const func_temp_t *temp,
                                int32_t value,
                                uint32_t now_ms)
{
    int64_t change;

    if ((temp->has_valid_publish == 0U) ||
        ((temp->has_published_quality != 0U) &&
         (temp->last_quality != FUNC_QUALITY_VALID))) {
        return 1U;
    }
    change = (int64_t)value - (int64_t)temp->last_published_value;
    if (change < 0) {
        change = -change;
    }
    if (change >= (int64_t)temp->cfg.publish_change_mdeg_c) {
        return 1U;
    }
    if ((temp->cfg.force_publish_period_ms != 0U) &&
        ((uint32_t)(now_ms - temp->last_valid_publish_ms) >=
         temp->cfg.force_publish_period_ms)) {
        return 1U;
    }
    return 0U;
}

sns_status_t func_temp_poll(void *ctx,
                            uint32_t now_ms,
                            func_sensor_event_t *event,
                            uint8_t *event_ready)
{
    func_temp_t *temp;
    int32_t raw;
    int32_t calibrated;
    int32_t filtered;
    sns_status_t status;

    if ((ctx == NULL) || (event == NULL) || (event_ready == NULL)) {
        return SNS_ERR_PARAM;
    }
    *event_ready = 0U;
    temp = (func_temp_t *)ctx;
    if (temp->initialized == 0U) {
        return SNS_ERR_STATE;
    }
    if ((temp->sample_started != 0U) &&
        ((uint32_t)(now_ms - temp->last_sample_ms) <
         temp->cfg.sample_period_ms)) {
        return SNS_OK;
    }
    temp->sample_started = 1U;
    temp->last_sample_ms = now_ms;

    status = temp->cfg.source->ops->read_mdeg_c(temp->cfg.source->ctx, &raw);
    if (status != SNS_OK) {
        return temp_error_event(temp, now_ms, status, event, event_ready);
    }
    if ((raw < FUNC_TEMP_MIN_MDEG_C) || (raw > FUNC_TEMP_MAX_MDEG_C)) {
        return temp_error_event(temp, now_ms, SNS_ERR_INVALID_DATA,
                                event, event_ready);
    }
    status = temp_calibrate(temp, raw, &calibrated);
    if (status != SNS_OK) {
        return temp_error_event(temp, now_ms, status, event, event_ready);
    }
    status = func_filter_chain_process(&temp->cfg.filters,
                                       calibrated,
                                       &filtered);
    if (status != SNS_OK) {
        return temp_error_event(temp, now_ms, status, event, event_ready);
    }

    temp->consecutive_errors = 0U;
    temp->last_valid_value = filtered;
    temp->has_valid = 1U;
    if (temp_publish_due(temp, filtered, now_ms) == 0U) {
        return SNS_OK;
    }
    temp_event_base(event, now_ms);
    event->value = filtered;
    event->quality = FUNC_QUALITY_VALID;
    event->status = SNS_OK;
    temp->last_published_value = filtered;
    temp->last_valid_publish_ms = now_ms;
    temp->last_quality = FUNC_QUALITY_VALID;
    temp->has_valid_publish = 1U;
    temp->has_published_quality = 1U;
    *event_ready = 1U;
    return SNS_OK;
}

sns_status_t func_temp_deinit(void *ctx)
{
    func_temp_t *temp;
    sns_status_t status;

    if (ctx == NULL) {
        return SNS_ERR_PARAM;
    }
    temp = (func_temp_t *)ctx;
    if (temp->initialized == 0U) {
        return SNS_ERR_STATE;
    }
    status = func_filter_chain_reset(&temp->cfg.filters);
    temp->initialized = 0U;
    return status;
}

const func_sensor_driver_ops_t func_temp_driver_ops = {
    func_temp_init,
    func_temp_poll,
    func_temp_deinit
};
