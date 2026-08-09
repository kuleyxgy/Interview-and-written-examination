#ifndef UTIL_MATH_H
#define UTIL_MATH_H

#include <stdint.h>

int64_t util_div_round_nearest_i64(int64_t numerator, int64_t denominator);
int32_t util_sat_i64_to_i32(int64_t value);

#endif
