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

    util_be16_write(bytes, UINT16_C(0xBEEF));

    TEST_ASSERT_EQ_U16(UINT16_C(0x00BE), bytes[0]);
    TEST_ASSERT_EQ_U16(UINT16_C(0x00EF), bytes[1]);
    TEST_ASSERT_EQ_U16(UINT16_C(0xBEEF), util_be16_read(bytes));
}

TEST(rounded_division_rounds_positive_and_negative_halves_away_from_zero)
{
    TEST_ASSERT_EQ_I32(3, (int32_t)util_div_round_nearest_i64(INT64_C(5), INT64_C(2)));
    TEST_ASSERT_EQ_I32(-3, (int32_t)util_div_round_nearest_i64(-INT64_C(5), INT64_C(2)));
    TEST_ASSERT_EQ_I32(4, (int32_t)util_div_round_nearest_i64(INT64_C(7), INT64_C(2)));
    TEST_ASSERT_EQ_I32(-4, (int32_t)util_div_round_nearest_i64(-INT64_C(7), INT64_C(2)));
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

static util_log_level_t captured_level;
static const char *captured_tag;
static const char *captured_message;

static void capture_log(util_log_level_t level, const char *tag, const char *message)
{
    captured_level = level;
    captured_tag = tag;
    captured_message = message;
}

TEST(log_sink_receives_level_tag_and_formatted_message)
{
    captured_tag = NULL;
    captured_message = NULL;
    util_log_init(capture_log);

    util_log_write(UTIL_LOG_LEVEL_WARN, "i2c", "read %u bytes", 7U);

    TEST_ASSERT_EQ_I32(UTIL_LOG_LEVEL_WARN, captured_level);
    TEST_ASSERT_STREQ("i2c", captured_tag);
    TEST_ASSERT_STREQ("read 7 bytes", captured_message);
}

int main(void)
{
    status_name_maps_known_and_unknown_codes();
    be16_round_trip_uses_network_order();
    rounded_division_rounds_positive_and_negative_halves_away_from_zero();
    saturation_clamps_both_int32_boundaries();
    ringbuf_drop_newest_preserves_existing_items_and_counts_drop();
    ringbuf_drop_oldest_replaces_oldest_item_and_counts_drop();
    log_sink_receives_level_tag_and_formatted_message();

    if (test_failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed\n", test_failures);
        return 1;
    }

    return 0;
}
