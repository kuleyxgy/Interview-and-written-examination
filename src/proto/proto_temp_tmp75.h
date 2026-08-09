#ifndef PROTO_TEMP_TMP75_H
#define PROTO_TEMP_TMP75_H

#include <stdint.h>

#include "proto_i2c_reg.h"
#include "proto_temp.h"
#include "util_status.h"

#define PROTO_TEMP_TMP75_TEMPERATURE_REGISTER UINT8_C(0x00)

typedef struct {
    proto_i2c_device_t *i2c;
} proto_temp_tmp75_t;

sns_status_t proto_temp_tmp75_init(proto_temp_tmp75_t *tmp75,
                                   proto_i2c_device_t *i2c);
sns_status_t proto_temp_tmp75_bind(proto_temp_tmp75_t *tmp75,
                                   proto_temp_device_t *device);
sns_status_t proto_temp_tmp75_decode_mdeg_c(const uint8_t *register_data,
                                            uint16_t capacity,
                                            int32_t *value_mdeg_c);

#endif
