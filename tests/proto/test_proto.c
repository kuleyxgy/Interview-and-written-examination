#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal_i2c.h"
#include "hal_time.h"
#include "host_i2c.h"
#include "host_time.h"
#include "proto_clock.h"
#include "proto_i2c_reg.h"
#include "proto_temp.h"
#include "proto_temp_tmp75.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    uint8_t reply[2];
    uint16_t address_seen;
    uint8_t register_seen;
    uint16_t write_length_seen;
    uint16_t read_capacity_seen;
    uint32_t timeout_seen;
    uint16_t calls;
} tmp75_fixture_t;

static sns_status_t tmp75_fixture_transfer(void *context,
                                            const uint8_t *write_data,
                                            uint16_t write_length,
                                            uint8_t *read_data,
                                            uint16_t read_capacity,
                                            uint32_t timeout_ms,
                                            uint16_t *transferred)
{
    tmp75_fixture_t *fixture = context;

    fixture->calls++;
    fixture->write_length_seen = write_length;
    fixture->read_capacity_seen = read_capacity;
    fixture->timeout_seen = timeout_ms;
    fixture->register_seen = (write_length > 0U) ? write_data[0] : UINT8_C(0xFF);

    if ((read_data == NULL) || (read_capacity < 2U) || (transferred == NULL)) {
        return SNS_ERR_PARAM;
    }

    read_data[0] = fixture->reply[0];
    read_data[1] = fixture->reply[1];
    *transferred = 2U;
    return SNS_OK;
}

TEST(proto_clock_observes_literal_time_and_unsigned_wrap)
{
    host_time_t host_clock;
    hal_time_t hal;
    proto_clock_t clock;
    uint32_t now_ms = UINT32_C(0xA5A5A5A5);

    TEST_ASSERT_EQ_I32(SNS_OK, host_time_init(&host_clock, UINT32_MAX - 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_time_bind(&host_clock, &hal));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_clock_init(&clock, &hal));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_clock_now_ms(&clock, &now_ms));
    TEST_ASSERT_EQ_U32(UINT32_MAX - 2U, now_ms);

    TEST_ASSERT_EQ_I32(SNS_OK, host_time_advance(&host_clock, 5U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_clock_now_ms(&clock, &now_ms));
    TEST_ASSERT_EQ_U32(2U, now_ms);
}

TEST(tmp75_decode_matches_hand_calculated_12_bit_vectors)
{
    static const struct {
        uint8_t bytes[2];
        int32_t expected_mdeg_c;
    } vectors[] = {
        { { UINT8_C(0x00), UINT8_C(0x00) }, 0 },
        { { UINT8_C(0x19), UINT8_C(0x00) }, 25000 },
        { { UINT8_C(0x19), UINT8_C(0x10) }, 25063 },
        { { UINT8_C(0xFF), UINT8_C(0xF0) }, -63 },
        { { UINT8_C(0xC9), UINT8_C(0x00) }, -55000 },
        { { UINT8_C(0x7D), UINT8_C(0x00) }, 125000 }
    };
    uint16_t index;

    for (index = 0U; index < (uint16_t)(sizeof(vectors) / sizeof(vectors[0])); ++index) {
        int32_t decoded = INT32_C(123456789);

        TEST_ASSERT_EQ_I32(SNS_OK,
                           proto_temp_tmp75_decode_mdeg_c(vectors[index].bytes, 2U, &decoded));
        TEST_ASSERT_EQ_I32(vectors[index].expected_mdeg_c, decoded);
    }
}

TEST(two_i2c_buses_drive_two_tmp75_instances_independently)
{
    tmp75_fixture_t first_fixture = {
        { UINT8_C(0x19), UINT8_C(0x00) }, 0U, 0U, 0U, 0U, 0U, 0U
    };
    tmp75_fixture_t second_fixture = {
        { UINT8_C(0xC9), UINT8_C(0x00) }, 0U, 0U, 0U, 0U, 0U, 0U
    };
    host_i2c_bus_t first_bus;
    host_i2c_bus_t second_bus;
    hal_i2c_t first_hal;
    hal_i2c_t second_hal;
    proto_i2c_device_t first_i2c;
    proto_i2c_device_t second_i2c;
    proto_temp_tmp75_t first_tmp75;
    proto_temp_tmp75_t second_tmp75;
    proto_temp_device_t first_temp;
    proto_temp_device_t second_temp;
    int32_t first_value = 0;
    int32_t second_value = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_init(&first_bus));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_init(&second_bus));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_bind(&first_bus, &first_hal));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_bind(&second_bus, &second_hal));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&first_bus, UINT8_C(0x48),
                                                      tmp75_fixture_transfer, &first_fixture));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&second_bus, UINT8_C(0x48),
                                                      tmp75_fixture_transfer, &second_fixture));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_i2c_device_init(&first_i2c, &first_hal,
                                                      UINT8_C(0x48), 7U, 11U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_i2c_device_init(&second_i2c, &second_hal,
                                                      UINT8_C(0x48), 7U, 29U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_init(&first_tmp75, &first_i2c));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_init(&second_tmp75, &second_i2c));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_bind(&first_tmp75, &first_temp));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_temp_tmp75_bind(&second_tmp75, &second_temp));

    TEST_ASSERT_EQ_I32(SNS_OK, first_temp.ops->read_mdeg_c(first_temp.ctx, &first_value));
    TEST_ASSERT_EQ_I32(SNS_OK, second_temp.ops->read_mdeg_c(second_temp.ctx, &second_value));

    TEST_ASSERT_EQ_I32(25000, first_value);
    TEST_ASSERT_EQ_I32(-55000, second_value);
    TEST_ASSERT_EQ_U16(1U, first_fixture.calls);
    TEST_ASSERT_EQ_U16(1U, second_fixture.calls);
    TEST_ASSERT_EQ_U16(1U, first_fixture.write_length_seen);
    TEST_ASSERT_EQ_U16(1U, second_fixture.write_length_seen);
    TEST_ASSERT_EQ_U16(2U, first_fixture.read_capacity_seen);
    TEST_ASSERT_EQ_U16(2U, second_fixture.read_capacity_seen);
    TEST_ASSERT_EQ_U16(UINT8_C(0x00), first_fixture.register_seen);
    TEST_ASSERT_EQ_U16(UINT8_C(0x00), second_fixture.register_seen);
    TEST_ASSERT_EQ_U32(11U, first_fixture.timeout_seen);
    TEST_ASSERT_EQ_U32(29U, second_fixture.timeout_seen);
}

int main(void)
{
    proto_clock_observes_literal_time_and_unsigned_wrap();
    tmp75_decode_matches_hand_calculated_12_bit_vectors();
    two_i2c_buses_drive_two_tmp75_instances_independently();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
