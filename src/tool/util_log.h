#ifndef UTIL_LOG_H
#define UTIL_LOG_H

typedef enum {
    UTIL_LOG_LEVEL_DEBUG = 0,
    UTIL_LOG_LEVEL_INFO,
    UTIL_LOG_LEVEL_WARN,
    UTIL_LOG_LEVEL_ERROR
} util_log_level_t;

typedef void (*util_log_sink_t)(util_log_level_t level, const char *tag, const char *message);

void util_log_init(util_log_sink_t sink);
void util_log_write(util_log_level_t level, const char *tag, const char *format, ...);

#endif
