#include "func_sensor.h"

#include <stddef.h>
#include <string.h>

static func_sensor_slot_t *find_sensor(func_sensor_core_t *core,
                                       func_sensor_id_t id)
{
    uint16_t index;

    for (index = 0U; index < core->sensor_count; ++index) {
        if (core->slots[index].registration.id == id) {
            return &core->slots[index];
        }
    }
    return NULL;
}

sns_status_t func_sensor_core_init(func_sensor_core_t *core)
{
    if (core == NULL) {
        return SNS_ERR_PARAM;
    }
    (void)memset(core, 0, sizeof(*core));
    core->initialized = 1U;
    return SNS_OK;
}

sns_status_t func_sensor_reset(func_sensor_core_t *core)
{
    uint16_t index;
    sns_status_t status;
    sns_status_t first_error;

    if ((core == NULL) || (core->initialized == 0U)) {
        return SNS_ERR_PARAM;
    }
    first_error = SNS_OK;
    for (index = 0U; index < core->sensor_count; ++index) {
        if (core->slots[index].registration.ops->deinit != NULL) {
            status = core->slots[index].registration.ops->deinit(
                core->slots[index].registration.driver_ctx);
            if ((first_error == SNS_OK) && (status != SNS_OK)) {
                first_error = status;
            }
        }
    }
    (void)memset(core->slots, 0, sizeof(core->slots));
    (void)memset(core->subscriptions, 0, sizeof(core->subscriptions));
    core->sensor_count = 0U;
    core->subscription_count = 0U;
    return first_error;
}

sns_status_t func_sensor_register(
    func_sensor_core_t *core,
    const func_sensor_registration_t *registration)
{
    func_sensor_slot_t *slot;
    sns_status_t status;

    if ((core == NULL) || (core->initialized == 0U) ||
        (registration == NULL) || (registration->name == NULL) ||
        (registration->ops == NULL) || (registration->ops->init == NULL) ||
        (registration->ops->poll == NULL) ||
        (registration->driver_ctx == NULL)) {
        return SNS_ERR_PARAM;
    }
    if (find_sensor(core, registration->id) != NULL) {
        return SNS_ERR_STATE;
    }
    if (core->sensor_count >= FUNC_CFG_MAX_SENSORS) {
        return SNS_ERR_NO_SPACE;
    }
    status = registration->ops->init(registration->driver_ctx);
    if (status != SNS_OK) {
        return status;
    }
    slot = &core->slots[core->sensor_count];
    slot->registration = *registration;
    slot->next_sequence = 0U;
    slot->has_latest = 0U;
    ++core->sensor_count;
    return SNS_OK;
}

sns_status_t func_sensor_subscribe(func_sensor_core_t *core,
                                   func_sensor_id_t sensor_id,
                                   func_event_queue_t *queue)
{
    uint16_t index;
    uint16_t ignored_count;
    sns_status_t status;

    if ((core == NULL) || (core->initialized == 0U)) {
        return SNS_ERR_PARAM;
    }
    if (find_sensor(core, sensor_id) == NULL) {
        return SNS_ERR_NOT_FOUND;
    }
    status = func_event_queue_count(queue, &ignored_count);
    if (status != SNS_OK) {
        return status;
    }
    for (index = 0U; index < core->subscription_count; ++index) {
        if ((core->subscriptions[index].sensor_id == sensor_id) &&
            (core->subscriptions[index].queue == queue)) {
            return SNS_ERR_STATE;
        }
    }
    if (core->subscription_count >= FUNC_CFG_MAX_SUBSCRIPTIONS) {
        return SNS_ERR_NO_SPACE;
    }
    core->subscriptions[core->subscription_count].sensor_id = sensor_id;
    core->subscriptions[core->subscription_count].queue = queue;
    ++core->subscription_count;
    return SNS_OK;
}

static sns_status_t publish_event(func_sensor_core_t *core,
                                  func_sensor_slot_t *slot,
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
    for (index = 0U; index < core->subscription_count; ++index) {
        if (core->subscriptions[index].sensor_id == event->sensor_id) {
            status = func_event_queue_push(core->subscriptions[index].queue,
                                           event);
            if ((first_error == SNS_OK) && (status != SNS_OK)) {
                first_error = status;
            }
        }
    }
    return first_error;
}

sns_status_t func_sensor_poll_all(func_sensor_core_t *core,
                                  uint32_t now_ms)
{
    uint16_t index;
    uint8_t event_ready;
    func_sensor_event_t event;
    sns_status_t status;
    sns_status_t publish_status;
    sns_status_t first_error;

    if ((core == NULL) || (core->initialized == 0U)) {
        return SNS_ERR_PARAM;
    }
    first_error = SNS_OK;
    for (index = 0U; index < core->sensor_count; ++index) {
        event_ready = 0U;
        status = core->slots[index].registration.ops->poll(
            core->slots[index].registration.driver_ctx,
            now_ms,
            &event,
            &event_ready);
        if (event_ready != 0U) {
            publish_status = publish_event(core, &core->slots[index], &event);
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

sns_status_t func_sensor_get_latest(func_sensor_core_t *core,
                                    func_sensor_id_t id,
                                    func_sensor_event_t *snapshot)
{
    func_sensor_slot_t *slot;

    if ((core == NULL) || (core->initialized == 0U) || (snapshot == NULL)) {
        return SNS_ERR_PARAM;
    }
    slot = find_sensor(core, id);
    if (slot == NULL) {
        return SNS_ERR_NOT_FOUND;
    }
    if (slot->has_latest == 0U) {
        return SNS_ERR_NOT_READY;
    }
    *snapshot = slot->latest;
    return SNS_OK;
}
