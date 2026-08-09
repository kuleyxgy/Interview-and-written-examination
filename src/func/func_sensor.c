#include "func_sensor.h"

#include <stddef.h>

#include "func_cfg.h"

typedef struct {
    func_sensor_registration_t registration;
    func_sensor_event_t latest;
    uint32_t next_sequence;
    uint8_t has_latest;
} func_sensor_slot_t;

typedef struct {
    func_sensor_id_t sensor_id;
    func_event_queue_t *queue;
} func_sensor_subscription_t;

static func_sensor_slot_t sensor_slots[FUNC_CFG_MAX_SENSORS];
static func_sensor_subscription_t
    sensor_subscriptions[FUNC_CFG_MAX_SUBSCRIPTIONS];
static uint16_t sensor_count;
static uint16_t subscription_count;

static func_sensor_slot_t *find_sensor(func_sensor_id_t id)
{
    uint16_t index;

    for (index = 0U; index < sensor_count; ++index) {
        if (sensor_slots[index].registration.id == id) {
            return &sensor_slots[index];
        }
    }
    return NULL;
}

sns_status_t func_sensor_reset(void)
{
    uint16_t index;
    sns_status_t status;
    sns_status_t first_error;

    first_error = SNS_OK;
    for (index = 0U; index < sensor_count; ++index) {
        if (sensor_slots[index].registration.ops->deinit != NULL) {
            status = sensor_slots[index].registration.ops->deinit(
                sensor_slots[index].registration.driver_ctx);
            if ((first_error == SNS_OK) && (status != SNS_OK)) {
                first_error = status;
            }
        }
    }
    sensor_count = 0U;
    subscription_count = 0U;
    return first_error;
}

sns_status_t func_sensor_register(
    const func_sensor_registration_t *registration)
{
    func_sensor_slot_t *slot;
    sns_status_t status;

    if ((registration == NULL) || (registration->name == NULL) ||
        (registration->ops == NULL) || (registration->ops->init == NULL) ||
        (registration->ops->poll == NULL) ||
        (registration->driver_ctx == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (find_sensor(registration->id) != NULL) {
        return SNS_ERR_STATE;
    }
    if (sensor_count >= FUNC_CFG_MAX_SENSORS) {
        return SNS_ERR_NO_SPACE;
    }
    status = registration->ops->init(registration->driver_ctx);
    if (status != SNS_OK) {
        return status;
    }
    slot = &sensor_slots[sensor_count];
    slot->registration = *registration;
    slot->next_sequence = 0U;
    slot->has_latest = 0U;
    ++sensor_count;
    return SNS_OK;
}

sns_status_t func_sensor_subscribe(func_sensor_id_t sensor_id,
                                   func_event_queue_t *queue)
{
    uint16_t index;
    uint16_t ignored_count;
    sns_status_t status;

    if (find_sensor(sensor_id) == NULL) {
        return SNS_ERR_NOT_FOUND;
    }
    status = func_event_queue_count(queue, &ignored_count);
    if (status != SNS_OK) {
        return status;
    }
    for (index = 0U; index < subscription_count; ++index) {
        if ((sensor_subscriptions[index].sensor_id == sensor_id) &&
            (sensor_subscriptions[index].queue == queue)) {
            return SNS_ERR_STATE;
        }
    }
    if (subscription_count >= FUNC_CFG_MAX_SUBSCRIPTIONS) {
        return SNS_ERR_NO_SPACE;
    }
    sensor_subscriptions[subscription_count].sensor_id = sensor_id;
    sensor_subscriptions[subscription_count].queue = queue;
    ++subscription_count;
    return SNS_OK;
}

static sns_status_t publish_event(func_sensor_slot_t *slot,
                                  func_sensor_event_t *event)
{
    uint16_t index;
    sns_status_t status;
    sns_status_t first_error;

    event->sensor_id = slot->registration.id;
    event->sequence = slot->next_sequence;
    ++slot->next_sequence;
    slot->latest = *event;
    slot->has_latest = 1U;

    first_error = SNS_OK;
    for (index = 0U; index < subscription_count; ++index) {
        if (sensor_subscriptions[index].sensor_id == event->sensor_id) {
            status = func_event_queue_push(sensor_subscriptions[index].queue,
                                           event);
            if ((first_error == SNS_OK) && (status != SNS_OK)) {
                first_error = status;
            }
        }
    }
    return first_error;
}

sns_status_t func_sensor_poll_all(uint32_t now_ms)
{
    uint16_t index;
    uint8_t event_ready;
    func_sensor_event_t event;
    sns_status_t status;
    sns_status_t publish_status;
    sns_status_t first_error;

    first_error = SNS_OK;
    for (index = 0U; index < sensor_count; ++index) {
        event_ready = 0U;
        status = sensor_slots[index].registration.ops->poll(
            sensor_slots[index].registration.driver_ctx,
            now_ms,
            &event,
            &event_ready);
        if (event_ready != 0U) {
            publish_status = publish_event(&sensor_slots[index], &event);
            if ((first_error == SNS_OK) && (publish_status != SNS_OK)) {
                first_error = publish_status;
            }
        }
        if ((first_error == SNS_OK) && (status != SNS_OK)) {
            first_error = status;
        }
    }
    return first_error;
}

sns_status_t func_sensor_get_latest(func_sensor_id_t id,
                                    func_sensor_event_t *snapshot)
{
    func_sensor_slot_t *slot;

    if (snapshot == NULL) {
        return SNS_ERR_PARAM;
    }
    slot = find_sensor(id);
    if (slot == NULL) {
        return SNS_ERR_NOT_FOUND;
    }
    if (slot->has_latest == 0U) {
        return SNS_ERR_NOT_READY;
    }
    *snapshot = slot->latest;
    return SNS_OK;
}
