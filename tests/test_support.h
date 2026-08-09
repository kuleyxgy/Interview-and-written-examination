#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_failures;

#define TEST(name) static void name(void)

#define TEST_ASSERT_TRUE(condition)                                                \
    do {                                                                           \
        if (!(condition)) {                                                        \
            (void)fprintf(stderr, "%s:%d: assertion failed: %s\n",              \
                          __FILE__, __LINE__, #condition);                        \
            test_failures++;                                                       \
        }                                                                          \
    } while (0)

#define TEST_ASSERT_EQ_I32(expected, actual)                                      \
    do {                                                                           \
        int32_t test_expected_ = (expected);                                      \
        int32_t test_actual_ = (actual);                                          \
        if (test_expected_ != test_actual_) {                                     \
            (void)fprintf(stderr, "%s:%d: expected %ld, got %ld\n",             \
                          __FILE__, __LINE__, (long)test_expected_,               \
                          (long)test_actual_);                                    \
            test_failures++;                                                       \
        }                                                                          \
    } while (0)

#define TEST_ASSERT_EQ_U16(expected, actual)                                      \
    do {                                                                           \
        uint16_t test_expected_ = (expected);                                     \
        uint16_t test_actual_ = (actual);                                         \
        if (test_expected_ != test_actual_) {                                     \
            (void)fprintf(stderr, "%s:%d: expected %u, got %u\n",                \
                          __FILE__, __LINE__, (unsigned int)test_expected_,       \
                          (unsigned int)test_actual_);                            \
            test_failures++;                                                       \
        }                                                                          \
    } while (0)

#define TEST_ASSERT_EQ_U32(expected, actual)                                      \
    do {                                                                           \
        uint32_t test_expected_ = (expected);                                     \
        uint32_t test_actual_ = (actual);                                         \
        if (test_expected_ != test_actual_) {                                     \
            (void)fprintf(stderr, "%s:%d: expected %lu, got %lu\n",              \
                          __FILE__, __LINE__, (unsigned long)test_expected_,      \
                          (unsigned long)test_actual_);                           \
            test_failures++;                                                       \
        }                                                                          \
    } while (0)

#define TEST_ASSERT_STREQ(expected, actual)                                       \
    do {                                                                           \
        const char *test_expected_ = (expected);                                  \
        const char *test_actual_ = (actual);                                      \
        if ((test_actual_ == NULL) || (strcmp(test_expected_, test_actual_) != 0)) { \
            (void)fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n",  \
                          __FILE__, __LINE__, test_expected_,                     \
                          (test_actual_ == NULL) ? "(null)" : test_actual_);     \
            test_failures++;                                                       \
        }                                                                          \
    } while (0)

#endif
