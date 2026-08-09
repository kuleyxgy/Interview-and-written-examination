#ifndef UTIL_RINGBUF_H
#define UTIL_RINGBUF_H

#include <stdint.h>

#include "util_status.h"

typedef enum {
    UTIL_RINGBUF_DROP_NEWEST = 0,
    UTIL_RINGBUF_DROP_OLDEST
} util_ringbuf_overflow_policy_t;

typedef struct {
    uint8_t *storage;
    uint16_t item_size;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t dropped;
    util_ringbuf_overflow_policy_t overflow_policy;
} util_ringbuf_t;

sns_status_t util_ringbuf_init(util_ringbuf_t *ringbuf,
                               void *storage,
                               uint16_t item_size,
                               uint16_t capacity,
                               util_ringbuf_overflow_policy_t overflow_policy);
sns_status_t util_ringbuf_push(util_ringbuf_t *ringbuf, const void *item);
sns_status_t util_ringbuf_pop(util_ringbuf_t *ringbuf, void *item);
uint16_t util_ringbuf_count(const util_ringbuf_t *ringbuf);
uint32_t util_ringbuf_dropped(const util_ringbuf_t *ringbuf);

#endif
