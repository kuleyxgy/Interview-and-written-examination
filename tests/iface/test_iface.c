#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal_i2c.h"
#include "hal_net.h"
#include "hal_time.h"
#include "host_i2c.h"
#include "host_net.h"
#include "host_time.h"
#include "test_support.h"
#include "util_status.h"

typedef struct {
    const uint8_t *reply;
    uint16_t reply_length;
    uint16_t transferred;
    sns_status_t result;
} fixture_i2c_device_t;

static sns_status_t fixture_i2c_transfer(void *context, const uint8_t *write_data,
                                         uint16_t write_length, uint8_t *read_data,
                                         uint16_t read_length, uint32_t timeout_ms,
                                         uint16_t *transferred)
{
    fixture_i2c_device_t *device = context;
    uint16_t copied = 0U;

    (void)write_data;
    (void)write_length;
    (void)timeout_ms;

    if (device->result != SNS_OK) {
        return device->result;
    }

    copied = device->transferred;
    if (copied > read_length) {
        copied = read_length;
    }
    if (copied > device->reply_length) {
        copied = device->reply_length;
    }
    if ((copied > 0U) && (read_data != NULL)) {
        (void)memcpy(read_data, device->reply, copied);
    }
    *transferred = copied;
    return SNS_OK;
}

TEST(two_host_clocks_advance_independently)
{
    host_time_t first_clock;
    host_time_t second_clock;
    hal_time_t first_hal;
    hal_time_t second_hal;
    uint32_t first_now = 0U;
    uint32_t second_now = 0U;

    host_time_init(&first_clock, 100U);
    host_time_init(&second_clock, 700U);
    host_time_bind(&first_clock, &first_hal);
    host_time_bind(&second_clock, &second_hal);
    host_time_advance(&first_clock, 23U);

    TEST_ASSERT_EQ_I32(SNS_OK, first_hal.ops->now_ms(first_hal.ctx, &first_now));
    TEST_ASSERT_EQ_I32(SNS_OK, second_hal.ops->now_ms(second_hal.ctx, &second_now));
    TEST_ASSERT_EQ_U32(123U, first_now);
    TEST_ASSERT_EQ_U32(700U, second_now);
}

TEST(two_i2c_buses_with_same_address_return_different_data)
{
    const uint8_t first_reply[] = { UINT8_C(0x12), UINT8_C(0x34) };
    const uint8_t second_reply[] = { UINT8_C(0xAB), UINT8_C(0xCD) };
    fixture_i2c_device_t first_device = { first_reply, 2U, 2U, SNS_OK };
    fixture_i2c_device_t second_device = { second_reply, 2U, 2U, SNS_OK };
    host_i2c_bus_t first_bus;
    host_i2c_bus_t second_bus;
    hal_i2c_t first_hal;
    hal_i2c_t second_hal;
    uint8_t first_received[2] = { 0U, 0U };
    uint8_t second_received[2] = { 0U, 0U };
    uint16_t first_transferred = 0U;
    uint16_t second_transferred = 0U;

    host_i2c_bus_init(&first_bus);
    host_i2c_bus_init(&second_bus);
    host_i2c_bus_bind(&first_bus, &first_hal);
    host_i2c_bus_bind(&second_bus, &second_hal);
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&first_bus, UINT8_C(0x48),
                                                      fixture_i2c_transfer, &first_device));
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&second_bus, UINT8_C(0x48),
                                                      fixture_i2c_transfer, &second_device));

    TEST_ASSERT_EQ_I32(SNS_OK, first_hal.ops->transfer(first_hal.ctx, UINT8_C(0x48), NULL, 0U,
                                                        first_received,
                                                        (uint16_t)sizeof(first_received), 10U,
                                                        &first_transferred));
    TEST_ASSERT_EQ_I32(SNS_OK, second_hal.ops->transfer(second_hal.ctx, UINT8_C(0x48), NULL, 0U,
                                                         second_received,
                                                         (uint16_t)sizeof(second_received), 10U,
                                                         &second_transferred));
    TEST_ASSERT_EQ_U16(2U, first_transferred);
    TEST_ASSERT_EQ_U16(2U, second_transferred);
    TEST_ASSERT_TRUE(memcmp(first_reply, first_received, sizeof(first_reply)) == 0);
    TEST_ASSERT_TRUE(memcmp(second_reply, second_received, sizeof(second_reply)) == 0);
}

TEST(i2c_propagates_nack_timeout_and_short_transfer)
{
    const uint8_t reply[] = { UINT8_C(0x5A), UINT8_C(0xA5) };
    fixture_i2c_device_t device = { reply, 2U, 2U, SNS_ERR_IO };
    host_i2c_bus_t bus;
    hal_i2c_t hal;
    uint8_t received[2] = { 0U, 0U };
    uint16_t transferred = UINT16_C(0xFFFF);

    host_i2c_bus_init(&bus);
    host_i2c_bus_bind(&bus, &hal);
    TEST_ASSERT_EQ_I32(SNS_OK, host_i2c_bus_register(&bus, UINT8_C(0x48), fixture_i2c_transfer,
                                                      &device));

    TEST_ASSERT_EQ_I32(SNS_ERR_IO, hal.ops->transfer(hal.ctx, UINT8_C(0x48), NULL, 0U, received,
                                                      (uint16_t)sizeof(received), 10U,
                                                      &transferred));
    device.result = SNS_ERR_TIMEOUT;
    TEST_ASSERT_EQ_I32(SNS_ERR_TIMEOUT, hal.ops->transfer(hal.ctx, UINT8_C(0x48), NULL, 0U,
                                                           received, (uint16_t)sizeof(received),
                                                           10U, &transferred));
    device.result = SNS_OK;
    device.transferred = 1U;
    TEST_ASSERT_EQ_I32(SNS_OK, hal.ops->transfer(hal.ctx, UINT8_C(0x48), NULL, 0U, received,
                                                 (uint16_t)sizeof(received), 10U, &transferred));
    TEST_ASSERT_EQ_U16(1U, transferred);
    TEST_ASSERT_EQ_U16(UINT16_C(0x005A), received[0]);
}

TEST(two_network_instances_capture_packets_independently)
{
    const uint8_t first_packet[] = { UINT8_C(0x10), UINT8_C(0x20) };
    const uint8_t second_packet[] = { UINT8_C(0xA0), UINT8_C(0xB0), UINT8_C(0xC0) };
    host_net_t first_host;
    host_net_t second_host;
    hal_net_t first_hal;
    hal_net_t second_hal;
    const uint8_t *captured = NULL;
    uint16_t captured_length = 0U;
    uint16_t sent = 0U;

    host_net_init(&first_host);
    host_net_init(&second_host);
    host_net_bind(&first_host, &first_hal);
    host_net_bind(&second_host, &second_hal);
    TEST_ASSERT_EQ_I32(SNS_OK, first_hal.ops->connect(first_hal.ctx, "first.example", 1883U, 50U));
    TEST_ASSERT_EQ_I32(SNS_OK, second_hal.ops->connect(second_hal.ctx, "second.example", 1884U,
                                                        50U));
    TEST_ASSERT_EQ_I32(SNS_OK, first_hal.ops->send(first_hal.ctx, first_packet,
                                                    (uint16_t)sizeof(first_packet), &sent));
    TEST_ASSERT_EQ_U16(2U, sent);
    TEST_ASSERT_EQ_I32(SNS_OK, second_hal.ops->send(second_hal.ctx, second_packet,
                                                     (uint16_t)sizeof(second_packet), &sent));
    TEST_ASSERT_EQ_U16(3U, sent);

    TEST_ASSERT_EQ_U16(1U, host_net_capture_count(&first_host));
    TEST_ASSERT_EQ_U16(1U, host_net_capture_count(&second_host));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_capture_at(&first_host, 0U, &captured, &captured_length));
    TEST_ASSERT_EQ_U16(2U, captured_length);
    TEST_ASSERT_TRUE(memcmp(first_packet, captured, sizeof(first_packet)) == 0);
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_capture_at(&second_host, 0U, &captured,
                                                    &captured_length));
    TEST_ASSERT_EQ_U16(3U, captured_length);
    TEST_ASSERT_TRUE(memcmp(second_packet, captured, sizeof(second_packet)) == 0);
}

TEST(network_disconnect_and_reconnect_are_observable)
{
    const uint8_t packet[] = { UINT8_C(0x33) };
    host_net_t host;
    hal_net_t hal;
    uint16_t sent = 0U;

    host_net_init(&host);
    host_net_bind(&host, &hal);

    TEST_ASSERT_TRUE(!host_net_is_connected(&host));
    TEST_ASSERT_EQ_I32(SNS_OK, hal.ops->connect(hal.ctx, "broker.example", 1883U, 50U));
    TEST_ASSERT_TRUE(host_net_is_connected(&host));
    TEST_ASSERT_EQ_I32(SNS_OK, hal.ops->close(hal.ctx));
    TEST_ASSERT_TRUE(!host_net_is_connected(&host));
    TEST_ASSERT_EQ_I32(SNS_ERR_STATE, hal.ops->send(hal.ctx, packet, (uint16_t)sizeof(packet),
                                                    &sent));
    TEST_ASSERT_EQ_I32(SNS_OK, hal.ops->connect(hal.ctx, "broker.example", 1883U, 50U));
    TEST_ASSERT_TRUE(host_net_is_connected(&host));
    TEST_ASSERT_EQ_I32(SNS_OK, hal.ops->send(hal.ctx, packet, (uint16_t)sizeof(packet), &sent));
    TEST_ASSERT_EQ_U16(1U, sent);
    TEST_ASSERT_EQ_U16(1U, host_net_capture_count(&host));
}

int main(void)
{
    two_host_clocks_advance_independently();
    two_i2c_buses_with_same_address_return_different_data();
    i2c_propagates_nack_timeout_and_short_transfer();
    two_network_instances_capture_packets_independently();
    network_disconnect_and_reconnect_are_observable();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
