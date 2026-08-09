#ifndef PROTO_CLOCK_H
#define PROTO_CLOCK_H

#include <stdint.h>

#include "hal_time.h"
#include "util_status.h"

typedef struct {
    hal_time_t *time;
} proto_clock_t;

sns_status_t proto_clock_init(proto_clock_t *clock, hal_time_t *time);
sns_status_t proto_clock_now_ms(proto_clock_t *clock, uint32_t *now_ms);

#endif
