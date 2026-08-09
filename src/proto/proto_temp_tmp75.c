#include "proto_temp_tmp75.h"

#include <stddef.h>
#include <stdint.h>

#include "util_byte.h"
#include "util_math.h"

static sns_status_t proto_temp_tmp75_ops_init(void *ctx)
{
    const proto_temp_tmp75_t *tmp75 = (const proto_temp_tmp75_t *)ctx;

    if ((tmp75 == NULL) || (tmp75->i2c == NULL)) {
        return SNS_ERR_PARAM;
    }

    return SNS_OK;
}

static sns_status_t proto_temp_tmp75_read_mdeg_c(void *ctx,
                                                 int32_t *value_mdeg_c)
{
    proto_temp_tmp75_t *tmp75 = (proto_temp_tmp75_t *)ctx;
    const uint8_t reg = PROTO_TEMP_TMP75_TEMPERATURE_REGISTER;
    uint8_t register_data[2];
    int32_t decoded;
    sns_status_t status;

    if ((tmp75 == NULL) || (tmp75->i2c == NULL) || (value_mdeg_c == NULL)) {
        return SNS_ERR_PARAM;
    }

    status = proto_i2c_reg_read(tmp75->i2c, &reg, 1U,
                                register_data,
                                (uint16_t)sizeof(register_data));
    if (status != SNS_OK) {
        return status;
    }

    status = proto_temp_tmp75_decode_mdeg_c(register_data,
                                            (uint16_t)sizeof(register_data),
                                            &decoded);
    if (status != SNS_OK) {
        return status;
    }

    *value_mdeg_c = decoded;
    return SNS_OK;
}

static const proto_temp_ops_t proto_temp_tmp75_ops = {
    proto_temp_tmp75_ops_init,
    proto_temp_tmp75_read_mdeg_c
};

sns_status_t proto_temp_tmp75_init(proto_temp_tmp75_t *tmp75,
                                   proto_i2c_device_t *i2c)
{
    if ((tmp75 == NULL) || (i2c == NULL)) {
        return SNS_ERR_PARAM;
    }

    tmp75->i2c = i2c;
    return SNS_OK;
}

sns_status_t proto_temp_tmp75_bind(proto_temp_tmp75_t *tmp75,
                                   proto_temp_device_t *device)
{
    if ((tmp75 == NULL) || (tmp75->i2c == NULL) || (device == NULL)) {
        return SNS_ERR_PARAM;
    }

    device->ops = &proto_temp_tmp75_ops;
    device->ctx = tmp75;
    return SNS_OK;
}

sns_status_t proto_temp_tmp75_decode_mdeg_c(const uint8_t *register_data,
                                            uint16_t capacity,
                                            int32_t *value_mdeg_c)
{
    uint16_t raw;
    uint16_t code_bits;
    int32_t signed_code;
    int64_t rounded;
    sns_status_t status;

    if ((register_data == NULL) || (capacity < 2U) ||
        (value_mdeg_c == NULL)) {
        return SNS_ERR_PARAM;
    }

    status = util_be16_read(register_data, capacity, &raw);
    if (status != SNS_OK) {
        return status;
    }

    code_bits = (uint16_t)(raw >> 4U);
    if ((code_bits & UINT16_C(0x0800)) != 0U) {
        signed_code = (int32_t)code_bits - INT32_C(4096);
    } else {
        signed_code = (int32_t)code_bits;
    }

    status = util_div_round_nearest_i64((int64_t)signed_code * INT64_C(625),
                                        INT64_C(10), &rounded);
    if (status != SNS_OK) {
        return status;
    }

    *value_mdeg_c = (int32_t)rounded;
    return SNS_OK;
}
