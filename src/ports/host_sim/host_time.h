#ifndef HOST_TIME_H
#define HOST_TIME_H

#include <stdint.h>

#include "hal_time.h"
#include "util_status.h"

typedef struct {
    uint32_t now_ms;
} host_time_t;

sns_status_t host_time_init(host_time_t *clock, uint32_t initial_ms);
sns_status_t host_time_bind(host_time_t *clock, hal_time_t *hal);
sns_status_t host_time_set(host_time_t *clock, uint32_t now_ms);
sns_status_t host_time_advance(host_time_t *clock, uint32_t elapsed_ms);

#endif
