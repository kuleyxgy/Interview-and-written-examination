#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <stdint.h>

#include "util_status.h"

typedef enum {
    UTIL_LOG_LEVEL_DEBUG = 0,
    UTIL_LOG_LEVEL_INFO,
    UTIL_LOG_LEVEL_WARN,
    UTIL_LOG_LEVEL_ERROR
} util_log_level_t;

typedef void (*util_log_sink_t)(void *context,
                                util_log_level_t level,
                                const char *tag,
                                const char *message);

typedef struct {
    util_log_sink_t sink;
    void *sink_context;
    char *buffer;
    uint16_t capacity;
} util_log_t;

sns_status_t util_log_init(util_log_t *log,
                           util_log_sink_t sink,
                           void *sink_context,
                           char *buffer,
                           uint16_t capacity);
sns_status_t util_log_write(util_log_t *log,
                            util_log_level_t level,
                            const char *tag,
                            const char *format,
                            ...);

#endif
