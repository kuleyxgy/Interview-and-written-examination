#include "func_event_queue.h"

#include <stddef.h>

static uint16_t queue_next(const func_event_queue_t *queue, uint16_t index)
{
    if (index == (uint16_t)(queue->capacity - 1U)) {
        return 0U;
    }
    return (uint16_t)(index + 1U);
}

static sns_status_t queue_valid(const func_event_queue_t *queue)
{
    if ((queue == NULL) || (queue->storage == NULL) ||
        (queue->capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    if ((queue->read_index >= queue->capacity) ||
        (queue->write_index >= queue->capacity) ||
        (queue->count > queue->capacity) ||
        ((queue->overflow_policy != FUNC_QUEUE_DROP_NEWEST) &&
         (queue->overflow_policy != FUNC_QUEUE_DROP_OLDEST))) {
        return SNS_ERR_STATE;
    }
    return SNS_OK;
}

sns_status_t func_event_queue_init(
    func_event_queue_t *queue,
    func_sensor_event_t *storage,
    uint16_t capacity,
    func_queue_overflow_policy_t overflow_policy)
{
    if ((queue == NULL) || (storage == NULL) || (capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    if ((overflow_policy != FUNC_QUEUE_DROP_NEWEST) &&
        (overflow_policy != FUNC_QUEUE_DROP_OLDEST)) {
        return SNS_ERR_PARAM;
    }
    queue->storage = storage;
    queue->capacity = capacity;
    queue->read_index = 0U;
    queue->write_index = 0U;
    queue->count = 0U;
    queue->dropped = 0U;
    queue->overflow_policy = overflow_policy;
    return SNS_OK;
}

sns_status_t func_event_queue_push(func_event_queue_t *queue,
                                   const func_sensor_event_t *event)
{
    sns_status_t status;

    if (event == NULL) {
        return SNS_ERR_PARAM;
    }
    status = queue_valid(queue);
    if (status != SNS_OK) {
        return status;
    }
    if (queue->count == queue->capacity) {
        ++queue->dropped;
        if (queue->overflow_policy == FUNC_QUEUE_DROP_NEWEST) {
            return SNS_ERR_NO_SPACE;
        }
        queue->read_index = queue_next(queue, queue->read_index);
        --queue->count;
    }
    queue->storage[queue->write_index] = *event;
    queue->write_index = queue_next(queue, queue->write_index);
    ++queue->count;
    return SNS_OK;
}

sns_status_t func_event_queue_pop(func_event_queue_t *queue,
                                  func_sensor_event_t *event)
{
    sns_status_t status;

    if (event == NULL) {
        return SNS_ERR_PARAM;
    }
    status = queue_valid(queue);
    if (status != SNS_OK) {
        return status;
    }
    if (queue->count == 0U) {
        return SNS_ERR_NOT_FOUND;
    }
    *event = queue->storage[queue->read_index];
    queue->read_index = queue_next(queue, queue->read_index);
    --queue->count;
    return SNS_OK;
}

sns_status_t func_event_queue_count(const func_event_queue_t *queue,
                                    uint16_t *count)
{
    sns_status_t status;

    if (count == NULL) {
        return SNS_ERR_PARAM;
    }
    status = queue_valid(queue);
    if (status != SNS_OK) {
        return status;
    }
    *count = queue->count;
    return SNS_OK;
}

sns_status_t func_event_queue_dropped(const func_event_queue_t *queue,
                                      uint32_t *dropped)
{
    sns_status_t status;

    if (dropped == NULL) {
        return SNS_ERR_PARAM;
    }
    status = queue_valid(queue);
    if (status != SNS_OK) {
        return status;
    }
    *dropped = queue->dropped;
    return SNS_OK;
}
