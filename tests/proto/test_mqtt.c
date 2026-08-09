#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal_net.h"
#include "host_net.h"
#include "proto_mqtt.h"
#include "test_support.h"
#include "util_status.h"

static const uint8_t golden_connect[] = {
    UINT8_C(0x10), UINT8_C(0x0E), UINT8_C(0x00), UINT8_C(0x04),
    UINT8_C(0x4D), UINT8_C(0x51), UINT8_C(0x54), UINT8_C(0x54),
    UINT8_C(0x04), UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x0A),
    UINT8_C(0x00), UINT8_C(0x02), UINT8_C(0x71), UINT8_C(0x61)
};

static const uint8_t golden_publish[] = {
    UINT8_C(0x30), UINT8_C(0x04), UINT8_C(0x00),
    UINT8_C(0x01), UINT8_C(0x74), UINT8_C(0x41)
};

static const uint8_t accepted_connack[] = {
    UINT8_C(0x20), UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x00)
};

static void init_host_client(proto_mqtt_client_t *client,
                             host_net_t *host,
                             hal_net_t *net,
                             proto_mqtt_packet_slot_t *slots,
                             uint16_t slot_count,
                             uint8_t *work,
                             uint16_t work_capacity)
{
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_init(host));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_bind(host, net));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_init(client, net, slots, slot_count,
                                                work, work_capacity));
}

TEST(connect_and_publish_wait_for_accepted_connack_with_golden_packets)
{
    host_net_t host;
    hal_net_t net;
    proto_mqtt_packet_slot_t slots[2];
    uint8_t work[64];
    proto_mqtt_client_t client;
    const uint8_t payload[] = { UINT8_C(0x41) };
    const uint8_t *captured = NULL;
    uint16_t length = 0U;

    init_host_client(&client, &host, &net, slots, 2U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_publish_enqueue(&client, "t", payload, 1U));

    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    TEST_ASSERT_EQ_U16(1U, host_net_capture_count(&host));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_capture_at(&host, 0U, &captured, &length));
    TEST_ASSERT_EQ_U16((uint16_t)sizeof(golden_connect), length);
    TEST_ASSERT_TRUE(memcmp(golden_connect, captured, sizeof(golden_connect)) == 0);

    TEST_ASSERT_EQ_I32(SNS_ERR_NOT_READY, proto_mqtt_poll(&client, 1U, 2U));
    TEST_ASSERT_EQ_U16(1U, host_net_capture_count(&host));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));

    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, accepted_connack,
                                                      (uint16_t)sizeof(accepted_connack)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 2U, 2U));
    TEST_ASSERT_TRUE(proto_mqtt_is_connected(&client));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 3U, 2U));
    TEST_ASSERT_EQ_U16(2U, host_net_capture_count(&host));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_capture_at(&host, 1U, &captured, &length));
    TEST_ASSERT_EQ_U16((uint16_t)sizeof(golden_publish), length);
    TEST_ASSERT_TRUE(memcmp(golden_publish, captured, sizeof(golden_publish)) == 0);

    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 10003U, 2U));
    TEST_ASSERT_EQ_U16(3U, host_net_capture_count(&host));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_capture_at(&host, 2U, &captured, &length));
    TEST_ASSERT_EQ_U16(2U, length);
    TEST_ASSERT_EQ_U16(UINT8_C(0xC0), captured[0]);
    TEST_ASSERT_EQ_U16(UINT8_C(0x00), captured[1]);
}

TEST(rejected_and_malformed_connack_keep_client_disconnected)
{
    static const uint8_t rejected[] = {
        UINT8_C(0x20), UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x05)
    };
    static const uint8_t malformed[] = {
        UINT8_C(0x20), UINT8_C(0x01), UINT8_C(0x00)
    };
    host_net_t host;
    hal_net_t net;
    proto_mqtt_packet_slot_t slot;
    uint8_t work[64];
    proto_mqtt_client_t client;

    init_host_client(&client, &host, &net, &slot, 1U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, rejected,
                                                      (uint16_t)sizeof(rejected)));
    TEST_ASSERT_TRUE(proto_mqtt_poll(&client, 1U, 2U) != SNS_OK);
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));

    init_host_client(&client, &host, &net, &slot, 1U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, malformed,
                                                      (uint16_t)sizeof(malformed)));
    TEST_ASSERT_EQ_I32(SNS_ERR_INVALID_DATA, proto_mqtt_poll(&client, 1U, 2U));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));
}

TEST(connack_accepts_one_plus_three_fragments_and_connack_pingresp_sticky_packet)
{
    static const uint8_t first_fragment[] = { UINT8_C(0x20) };
    static const uint8_t second_fragment[] = {
        UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x00)
    };
    static const uint8_t sticky[] = {
        UINT8_C(0x20), UINT8_C(0x02), UINT8_C(0x00), UINT8_C(0x00),
        UINT8_C(0xD0), UINT8_C(0x00)
    };
    host_net_t host;
    hal_net_t net;
    proto_mqtt_packet_slot_t slot;
    uint8_t work[64];
    proto_mqtt_client_t client;

    init_host_client(&client, &host, &net, &slot, 1U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, first_fragment, 1U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1U, 2U));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, second_fragment, 3U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 2U, 2U));
    TEST_ASSERT_TRUE(proto_mqtt_is_connected(&client));

    init_host_client(&client, &host, &net, &slot, 1U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    client.awaiting_ping_response = 1U;
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, sticky,
                                                      (uint16_t)sizeof(sticky)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1U, 2U));
    TEST_ASSERT_TRUE(proto_mqtt_is_connected(&client));
    TEST_ASSERT_EQ_U16(0U, client.awaiting_ping_response);
}

TEST(invalid_reconfigure_is_atomic_and_queue_full_counts_drop)
{
    host_net_t host;
    hal_net_t net;
    proto_mqtt_packet_slot_t slot;
    uint8_t work[64];
    proto_mqtt_client_t client;
    const uint8_t payload[] = { UINT8_C(0x41) };

    init_host_client(&client, &host, &net, &slot, 1U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "old.example", 1883U, "old-id",
                                                   10U, 50U, 0U));
    TEST_ASSERT_TRUE(proto_mqtt_connect(&client, "new.example", 1884U, "",
                                        20U, 70U, 1U) != SNS_OK);
    TEST_ASSERT_STREQ("old.example", client.host);
    TEST_ASSERT_STREQ("old-id", client.client_id);
    TEST_ASSERT_EQ_U16(1883U, client.port);
    TEST_ASSERT_EQ_U16(10U, client.keepalive_seconds);
    TEST_ASSERT_EQ_U32(50U, client.connect_timeout_ms);

    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_publish_enqueue(&client, "t", payload, 1U));
    TEST_ASSERT_EQ_I32(SNS_ERR_NO_SPACE,
                       proto_mqtt_publish_enqueue(&client, "t", payload, 1U));
    TEST_ASSERT_EQ_U16(1U, proto_mqtt_pending(&client));
    TEST_ASSERT_EQ_U32(1U, proto_mqtt_dropped(&client));
}

typedef struct {
    uint8_t rx[8];
    uint16_t rx_length;
    uint8_t rx_ready;
    uint16_t send_limit;
    sns_status_t next_send_status;
    uint8_t sent_data[12][64];
    uint16_t sent_length[12];
    uint16_t send_calls;
    uint16_t recv_calls;
    uint16_t connect_calls;
    uint16_t close_calls;
    uint16_t work_calls;
    uint32_t last_connect_timeout_ms;
} scripted_net_t;

static sns_status_t scripted_connect(void *context, const char *host,
                                     uint16_t port, uint32_t timeout_ms)
{
    scripted_net_t *script = context;
    (void)host;
    (void)port;
    (void)timeout_ms;
    script->connect_calls++;
    script->work_calls++;
    script->last_connect_timeout_ms = timeout_ms;
    return SNS_OK;
}

static sns_status_t scripted_send(void *context, const uint8_t *data,
                                  uint16_t length, uint16_t *sent)
{
    scripted_net_t *script = context;
    uint16_t amount = length;

    script->work_calls++;
    if (script->next_send_status != SNS_OK) {
        sns_status_t result = script->next_send_status;
        script->next_send_status = SNS_OK;
        return result;
    }
    if ((script->send_limit != 0U) && (amount > script->send_limit)) {
        amount = script->send_limit;
    }
    if (script->send_calls < 12U) {
        (void)memcpy(script->sent_data[script->send_calls], data, amount);
        script->sent_length[script->send_calls] = amount;
    }
    script->send_calls++;
    *sent = amount;
    return SNS_OK;
}

static sns_status_t scripted_recv(void *context, uint8_t *data,
                                  uint16_t capacity, uint16_t *received)
{
    scripted_net_t *script = context;
    script->recv_calls++;
    script->work_calls++;
    if (script->rx_ready == 0U) {
        return SNS_ERR_NOT_READY;
    }
    if (capacity < script->rx_length) {
        return SNS_ERR_NO_SPACE;
    }
    (void)memcpy(data, script->rx, script->rx_length);
    *received = script->rx_length;
    script->rx_ready = 0U;
    return SNS_OK;
}

static sns_status_t scripted_close(void *context)
{
    scripted_net_t *script = context;
    script->close_calls++;
    return SNS_OK;
}

static const hal_net_ops_t scripted_ops = {
    scripted_connect,
    scripted_send,
    scripted_recv,
    scripted_close
};

static void script_connack(scripted_net_t *script)
{
    (void)memcpy(script->rx, accepted_connack, sizeof(accepted_connack));
    script->rx_length = (uint16_t)sizeof(accepted_connack);
    script->rx_ready = 1U;
}

TEST(partial_publish_restarts_from_fixed_header_after_reconnect)
{
    scripted_net_t script;
    hal_net_t net = { &scripted_ops, &script };
    proto_mqtt_packet_slot_t slots[2];
    uint8_t work[64];
    proto_mqtt_client_t client;
    const uint8_t payload[] = { UINT8_C(0x41) };
    uint16_t publish_retry_call;

    (void)memset(&script, 0, sizeof(script));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_init(&client, &net, slots, 2U,
                                                work, (uint16_t)sizeof(work)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    script_connack(&script);
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_publish_enqueue(&client, "t", payload, 1U));

    script.send_limit = 2U;
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 2U, 2U));
    script.send_limit = 0U;
    script.next_send_status = SNS_ERR_IO;
    TEST_ASSERT_EQ_I32(SNS_ERR_IO, proto_mqtt_poll(&client, 3U, 2U));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));

    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1003U, 2U));
    TEST_ASSERT_EQ_U32(2U, script.last_connect_timeout_ms);
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1004U, 2U));
    script_connack(&script);
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1005U, 2U));
    publish_retry_call = script.send_calls;
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1006U, 2U));
    TEST_ASSERT_EQ_U16((uint16_t)(publish_retry_call + 1U), script.send_calls);
    TEST_ASSERT_EQ_U16((uint16_t)sizeof(golden_publish),
                       script.sent_length[publish_retry_call]);
    TEST_ASSERT_TRUE(memcmp(golden_publish, script.sent_data[publish_retry_call],
                            sizeof(golden_publish)) == 0);
}

TEST(one_millisecond_budget_allows_at_most_one_hal_work_unit)
{
    scripted_net_t script;
    hal_net_t net = { &scripted_ops, &script };
    proto_mqtt_packet_slot_t slot;
    uint8_t work[64];
    proto_mqtt_client_t client;
    const uint8_t payload[] = { UINT8_C(0x41) };

    (void)memset(&script, 0, sizeof(script));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_init(&client, &net, &slot, 1U,
                                                work, (uint16_t)sizeof(work)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 1U));
    script_connack(&script);
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 1U, 1U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_publish_enqueue(&client, "t", payload, 1U));

    script.work_calls = 0U;
    (void)proto_mqtt_poll(&client, 2U, 1U);
    TEST_ASSERT_TRUE(script.work_calls <= 1U);
}

TEST(connack_timeout_starts_after_partial_connect_packet_finishes)
{
    scripted_net_t script;
    hal_net_t net = { &scripted_ops, &script };
    proto_mqtt_packet_slot_t slot;
    uint8_t work[64];
    proto_mqtt_client_t client;

    (void)memset(&script, 0, sizeof(script));
    script.send_limit = 4U;
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_init(&client, &net, &slot, 1U,
                                                work, (uint16_t)sizeof(work)));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 5U, 100U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 101U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 103U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 105U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 107U, 2U));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));

    script_connack(&script);
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 111U, 2U));
    TEST_ASSERT_TRUE(proto_mqtt_is_connected(&client));
}

TEST(clean_session_rejects_connack_with_session_present)
{
    static const uint8_t invalid_connack[] = {
        UINT8_C(0x20), UINT8_C(0x02), UINT8_C(0x01), UINT8_C(0x00)
    };
    host_net_t host;
    hal_net_t net;
    proto_mqtt_packet_slot_t slot;
    uint8_t work[64];
    proto_mqtt_client_t client;

    init_host_client(&client, &host, &net, &slot, 1U, work, (uint16_t)sizeof(work));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_connect(&client, "broker", 1883U, "qa",
                                                   10U, 50U, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, proto_mqtt_poll(&client, 0U, 2U));
    TEST_ASSERT_EQ_I32(SNS_OK, host_net_receive_push(&host, invalid_connack,
                                                      (uint16_t)sizeof(invalid_connack)));
    TEST_ASSERT_EQ_I32(SNS_ERR_INVALID_DATA, proto_mqtt_poll(&client, 1U, 2U));
    TEST_ASSERT_TRUE(!proto_mqtt_is_connected(&client));
}

int main(void)
{
    connect_and_publish_wait_for_accepted_connack_with_golden_packets();
    rejected_and_malformed_connack_keep_client_disconnected();
    connack_accepts_one_plus_three_fragments_and_connack_pingresp_sticky_packet();
    invalid_reconfigure_is_atomic_and_queue_full_counts_drop();
    partial_publish_restarts_from_fixed_header_after_reconnect();
    one_millisecond_budget_allows_at_most_one_hal_work_unit();
    connack_timeout_starts_after_partial_connect_packet_finishes();
    clean_session_rejects_connack_with_session_present();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
