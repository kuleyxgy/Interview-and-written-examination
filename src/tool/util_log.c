#include "util_log.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

sns_status_t util_log_init(util_log_t *log,
                           util_log_sink_t sink,
                           void *sink_context,
                           char *buffer,
                           uint16_t capacity)
{
    if ((log == NULL) || (sink == NULL) || (buffer == NULL) || (capacity == 0U)) {
        return SNS_ERR_PARAM;
    }

    log->sink = sink;
    log->sink_context = sink_context;
    log->buffer = buffer;
    log->capacity = capacity;
    log->buffer[0] = '\0';

    return SNS_OK;
}

sns_status_t util_log_write(util_log_t *log,
                            util_log_level_t level,
                            const char *tag,
                            const char *format,
                            ...)
{
    va_list arguments;
    int format_result;

    if ((log == NULL) || (tag == NULL) || (format == NULL)) {
        return SNS_ERR_PARAM;
    }
    if ((log->sink == NULL) || (log->buffer == NULL) || (log->capacity == 0U)) {
        return SNS_ERR_PARAM;
    }

    log->buffer[0] = '\0';
    va_start(arguments, format);
    format_result = vsnprintf(log->buffer, (size_t)log->capacity, format, arguments);
    va_end(arguments);
    log->buffer[log->capacity - 1U] = '\0';

    if (format_result < 0) {
        log->buffer[0] = '\0';
        return SNS_ERR_INVALID_DATA;
    }

    log->sink(log->sink_context, level, tag, log->buffer);
    return SNS_OK;
}
