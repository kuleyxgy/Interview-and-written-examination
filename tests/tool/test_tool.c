#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "test_support.h"
#include "util_byte.h"
#include "util_log.h"
#include "util_math.h"
#include "util_ringbuf.h"
#include "util_status.h"

TEST(status_name_maps_known_and_unknown_codes)
{
    TEST_ASSERT_STREQ("OK", sns_status_name(SNS_OK));
    TEST_ASSERT_STREQ("INVALID_DATA", sns_status_name(SNS_ERR_INVALID_DATA));
    TEST_ASSERT_STREQ("UNKNOWN", sns_status_name((sns_status_t)-99));
}

TEST(be16_round_trip_uses_network_order)
{
    uint8_t bytes[2] = { 0U, 0U };
    uint16_t value = 0U;

    TEST_ASSERT_EQ_I32(SNS_OK, util_be16_write(bytes, (uint16_t)sizeof(bytes), UINT16_C(0xBEEF)));

    TEST_ASSERT_EQ_U16(UINT16_C(0x00BE), bytes[0]);
    TEST_ASSERT_EQ_U16(UINT16_C(0x00EF), bytes[1]);
    TEST_ASSERT_EQ_I32(SNS_OK, util_be16_read(bytes, (uint16_t)sizeof(bytes), &value));
    TEST_ASSERT_EQ_U16(UINT16_C(0xBEEF), value);
}

TEST(be16_rejects_null_and_short_buffers_without_changing_output)
{
    uint8_t bytes[2] = { UINT8_C(0x12), UINT8_C(0x34) };
    uint16_t value = UINT16_C(0xA55A);

    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_be16_read(NULL, 2U, &value));
    TEST_ASSERT_EQ_U16(UINT16_C(0xA55A), value);
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_be16_read(bytes, 1U, &value));
    TEST_ASSERT_EQ_U16(UINT16_C(0xA55A), value);
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_be16_read(bytes, 2U, NULL));

    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_be16_write(NULL, 2U, UINT16_C(0xBEEF)));
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_be16_write(bytes, 1U, UINT16_C(0xBEEF)));
    TEST_ASSERT_EQ_U16(UINT16_C(0x0012), bytes[0]);
    TEST_ASSERT_EQ_U16(UINT16_C(0x0034), bytes[1]);
}

TEST(rounded_division_rounds_positive_and_negative_halves_away_from_zero)
{
    int64_t result = 0;

    TEST_ASSERT_EQ_I32(SNS_OK, util_div_round_nearest_i64(INT64_C(5), INT64_C(2), &result));
    TEST_ASSERT_EQ_I32(3, (int32_t)result);
    TEST_ASSERT_EQ_I32(SNS_OK, util_div_round_nearest_i64(-INT64_C(5), INT64_C(2), &result));
    TEST_ASSERT_EQ_I32(-3, (int32_t)result);
    TEST_ASSERT_EQ_I32(SNS_OK, util_div_round_nearest_i64(INT64_C(7), INT64_C(2), &result));
    TEST_ASSERT_EQ_I32(4, (int32_t)result);
    TEST_ASSERT_EQ_I32(SNS_OK, util_div_round_nearest_i64(-INT64_C(7), INT64_C(2), &result));
    TEST_ASSERT_EQ_I32(-4, (int32_t)result);
}

TEST(rounded_division_rejects_invalid_arguments_without_changing_output)
{
    int64_t result = INT64_C(123456);

    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_div_round_nearest_i64(INT64_C(5), 0, &result));
    TEST_ASSERT_EQ_I32(123456, (int32_t)result);
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_div_round_nearest_i64(INT64_C(5), INT64_C(2), NULL));
}

TEST(saturation_clamps_both_int32_boundaries)
{
    TEST_ASSERT_EQ_I32(INT32_MAX, util_sat_i64_to_i32(INT64_C(2147483648)));
    TEST_ASSERT_EQ_I32(INT32_MIN, util_sat_i64_to_i32(-INT64_C(2147483649)));
    TEST_ASSERT_EQ_I32(42, util_sat_i64_to_i32(INT64_C(42)));
}

TEST(ringbuf_drop_newest_preserves_existing_items_and_counts_drop)
{
    uint16_t storage[3] = { 0U, 0U, 0U };
    uint16_t independent_storage[1] = { 0U };
    uint16_t item = 0U;
    uint16_t first = 10U;
    uint16_t second = 20U;
    uint16_t third = 30U;
    uint16_t rejected = 40U;
    uint16_t independent_item = 77U;
    util_ringbuf_t ringbuf;
    util_ringbuf_t independent_ringbuf;

    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_init(&ringbuf, storage, (uint16_t)sizeof(storage[0]),
                                                  3U, UTIL_RINGBUF_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &first));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_init(&independent_ringbuf, independent_storage,
                                                  (uint16_t)sizeof(independent_storage[0]), 1U,
                                                  UTIL_RINGBUF_DROP_NEWEST));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&independent_ringbuf, &independent_item));
    first = 99U;
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &second));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &third));
    TEST_ASSERT_EQ_I32(SNS_ERR_NO_SPACE, util_ringbuf_push(&ringbuf, &rejected));
    TEST_ASSERT_EQ_U32(1U, util_ringbuf_dropped(&ringbuf));
    TEST_ASSERT_EQ_U16(3U, util_ringbuf_count(&ringbuf));

    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&ringbuf, &item));
    TEST_ASSERT_EQ_U16(10U, item);
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&ringbuf, &item));
    TEST_ASSERT_EQ_U16(20U, item);
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&ringbuf, &item));
    TEST_ASSERT_EQ_U16(30U, item);
    TEST_ASSERT_EQ_U16(1U, util_ringbuf_count(&independent_ringbuf));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&independent_ringbuf, &item));
    TEST_ASSERT_EQ_U16(77U, item);
    TEST_ASSERT_EQ_U16(0U, util_ringbuf_count(&independent_ringbuf));
}

TEST(ringbuf_drop_oldest_replaces_oldest_item_and_counts_drop)
{
    uint16_t storage[3] = { 0U, 0U, 0U };
    uint16_t item = 0U;
    uint16_t first = 10U;
    uint16_t second = 20U;
    uint16_t third = 30U;
    uint16_t replacement = 40U;
    util_ringbuf_t ringbuf;

    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_init(&ringbuf, storage, (uint16_t)sizeof(storage[0]),
                                                  3U, UTIL_RINGBUF_DROP_OLDEST));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &first));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &second));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &third));
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_push(&ringbuf, &replacement));
    TEST_ASSERT_EQ_U32(1U, util_ringbuf_dropped(&ringbuf));
    TEST_ASSERT_EQ_U16(3U, util_ringbuf_count(&ringbuf));

    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&ringbuf, &item));
    TEST_ASSERT_EQ_U16(20U, item);
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&ringbuf, &item));
    TEST_ASSERT_EQ_U16(30U, item);
    TEST_ASSERT_EQ_I32(SNS_OK, util_ringbuf_pop(&ringbuf, &item));
    TEST_ASSERT_EQ_U16(40U, item);
}

typedef struct {
    util_log_level_t level;
    const char *tag;
    const char *message;
    void *context;
} captured_log_t;

static void capture_log(void *context, util_log_level_t level, const char *tag, const char *message)
{
    captured_log_t *capture = context;

    capture->level = level;
    capture->tag = tag;
    capture->message = message;
    capture->context = context;
}

TEST(log_sink_receives_level_tag_and_formatted_message)
{
    char line_buffer[32] = { '\0' };
    captured_log_t capture = { UTIL_LOG_LEVEL_DEBUG, NULL, NULL, NULL };
    util_log_t log;

    TEST_ASSERT_EQ_I32(SNS_OK, util_log_init(&log, capture_log, &capture, line_buffer,
                                              (uint16_t)sizeof(line_buffer)));

    TEST_ASSERT_EQ_I32(SNS_OK, util_log_write(&log, UTIL_LOG_LEVEL_WARN, "i2c", "read %u bytes", 7U));

    TEST_ASSERT_EQ_I32(UTIL_LOG_LEVEL_WARN, capture.level);
    TEST_ASSERT_TRUE(capture.context == &capture);
    TEST_ASSERT_STREQ("i2c", capture.tag);
    TEST_ASSERT_STREQ("read 7 bytes", capture.message);
    TEST_ASSERT_STREQ("read 7 bytes", line_buffer);
}

TEST(log_rejects_empty_context_buffer_and_capacity)
{
    char line_buffer[16] = { '\0' };
    captured_log_t capture = { UTIL_LOG_LEVEL_DEBUG, NULL, NULL, NULL };
    util_log_t log;

    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_log_init(NULL, capture_log, &capture, line_buffer,
                                                     (uint16_t)sizeof(line_buffer)));
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_log_init(&log, NULL, &capture, line_buffer,
                                                     (uint16_t)sizeof(line_buffer)));
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_log_init(&log, capture_log, &capture, NULL,
                                                     (uint16_t)sizeof(line_buffer)));
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_log_init(&log, capture_log, &capture, line_buffer, 0U));
    TEST_ASSERT_EQ_I32(SNS_OK, util_log_init(&log, capture_log, &capture, line_buffer,
                                              (uint16_t)sizeof(line_buffer)));
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_log_write(NULL, UTIL_LOG_LEVEL_INFO, "tool", "ready"));
    TEST_ASSERT_EQ_I32(SNS_ERR_PARAM, util_log_write(&log, UTIL_LOG_LEVEL_INFO, "tool", NULL));
}

int main(void)
{
    status_name_maps_known_and_unknown_codes();
    be16_round_trip_uses_network_order();
    be16_rejects_null_and_short_buffers_without_changing_output();
    rounded_division_rounds_positive_and_negative_halves_away_from_zero();
    rounded_division_rejects_invalid_arguments_without_changing_output();
    saturation_clamps_both_int32_boundaries();
    ringbuf_drop_newest_preserves_existing_items_and_counts_drop();
    ringbuf_drop_oldest_replaces_oldest_item_and_counts_drop();
    log_sink_receives_level_tag_and_formatted_message();
    log_rejects_empty_context_buffer_and_capacity();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
