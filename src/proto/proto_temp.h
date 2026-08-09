#ifndef PROTO_TEMP_H
#define PROTO_TEMP_H

#include <stdint.h>

#include "util_status.h"

typedef struct proto_temp_device proto_temp_device_t;

typedef struct {
    sns_status_t (*init)(void *ctx);
    sns_status_t (*read_mdeg_c)(void *ctx, int32_t *value_mdeg_c);
} proto_temp_ops_t;

struct proto_temp_device {
    const proto_temp_ops_t *ops;
    void *ctx;
};

#endif
