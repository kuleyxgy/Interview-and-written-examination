#include "util_ringbuf.h"

#include <stddef.h>
#include <string.h>

static uint16_t util_ringbuf_next_index(const util_ringbuf_t *ringbuf, uint16_t index)
{
    if (index == (uint16_t)(ringbuf->capacity - 1U)) {
        return 0U;
    }

    return (uint16_t)(index + 1U);
}

static uint8_t *util_ringbuf_item_at(const util_ringbuf_t *ringbuf, uint16_t index)
{
    return ringbuf->storage + ((size_t)index * ringbuf->item_size);
}

sns_status_t util_ringbuf_init(util_ringbuf_t *ringbuf,
                               void *storage,
                               uint16_t item_size,
                               uint16_t capacity,
                               util_ringbuf_overflow_policy_t overflow_policy)
{
    if ((ringbuf == NULL) || (storage == NULL) || (item_size == 0U) || (capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    if ((overflow_policy != UTIL_RINGBUF_DROP_NEWEST) &&
        (overflow_policy != UTIL_RINGBUF_DROP_OLDEST)) {
        return SNS_ERR_PARAM;
    }

    ringbuf->storage = (uint8_t *)storage;
    ringbuf->item_size = item_size;
    ringbuf->capacity = capacity;
    ringbuf->head = 0U;
    ringbuf->tail = 0U;
    ringbuf->count = 0U;
    ringbuf->dropped = 0U;
    ringbuf->overflow_policy = overflow_policy;

    return SNS_OK;
}

sns_status_t util_ringbuf_push(util_ringbuf_t *ringbuf, const void *item)
{
    if ((ringbuf == NULL) || (item == NULL) || (ringbuf->storage == NULL) ||
        (ringbuf->item_size == 0U) || (ringbuf->capacity == 0U)) {
        return SNS_ERR_PARAM;
    }

    if (ringbuf->count == ringbuf->capacity) {
        ringbuf->dropped++;
        if (ringbuf->overflow_policy == UTIL_RINGBUF_DROP_NEWEST) {
            return SNS_ERR_NO_SPACE;
        }
        if (ringbuf->overflow_policy != UTIL_RINGBUF_DROP_OLDEST) {
            return SNS_ERR_STATE;
        }
        ringbuf->head = util_ringbuf_next_index(ringbuf, ringbuf->head);
        ringbuf->count--;
    }

    (void)memcpy(util_ringbuf_item_at(ringbuf, ringbuf->tail), item, ringbuf->item_size);
    ringbuf->tail = util_ringbuf_next_index(ringbuf, ringbuf->tail);
    ringbuf->count++;

    return SNS_OK;
}

sns_status_t util_ringbuf_pop(util_ringbuf_t *ringbuf, void *item)
{
    if ((ringbuf == NULL) || (item == NULL) || (ringbuf->storage == NULL) ||
        (ringbuf->item_size == 0U) || (ringbuf->capacity == 0U)) {
        return SNS_ERR_PARAM;
    }
    if (ringbuf->count == 0U) {
        return SNS_ERR_NOT_FOUND;
    }

    (void)memcpy(item, util_ringbuf_item_at(ringbuf, ringbuf->head), ringbuf->item_size);
    ringbuf->head = util_ringbuf_next_index(ringbuf, ringbuf->head);
    ringbuf->count--;

    return SNS_OK;
}

uint16_t util_ringbuf_count(const util_ringbuf_t *ringbuf)
{
    if (ringbuf == NULL) {
        return 0U;
    }

    return ringbuf->count;
}

uint32_t util_ringbuf_dropped(const util_ringbuf_t *ringbuf)
{
    if (ringbuf == NULL) {
        return 0U;
    }

    return ringbuf->dropped;
}
