#include "util_math.h"

#include <limits.h>
#include <stdint.h>

static uint64_t util_abs_i64_to_u64(int64_t value)
{
    if (value < 0) {
        return UINT64_C(0) - (uint64_t)value;
    }

    return (uint64_t)value;
}

int64_t util_div_round_nearest_i64(int64_t numerator, int64_t denominator)
{
    int64_t quotient;
    int64_t remainder;
    uint64_t threshold;

    if (denominator == 0) {
        return 0;
    }
    if ((numerator == INT64_MIN) && (denominator == -1)) {
        return INT64_MAX;
    }

    quotient = numerator / denominator;
    remainder = numerator % denominator;
    threshold = (util_abs_i64_to_u64(denominator) / 2U) +
                (util_abs_i64_to_u64(denominator) % 2U);

    if ((remainder != 0) && (util_abs_i64_to_u64(remainder) >= threshold)) {
        if ((numerator < 0) != (denominator < 0)) {
            quotient--;
        } else {
            quotient++;
        }
    }

    return quotient;
}

int32_t util_sat_i64_to_i32(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }

    return (int32_t)value;
}
