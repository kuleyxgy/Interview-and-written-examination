#include "util_log.h"

#include <stdarg.h>
#include <stdio.h>

#include "util_cfg.h"

static util_log_sink_t util_log_sink;
static char util_log_line[UTIL_CFG_LOG_LINE_MAX];

void util_log_init(util_log_sink_t sink)
{
    util_log_sink = sink;
}

void util_log_write(util_log_level_t level, const char *tag, const char *format, ...)
{
    va_list arguments;

    if ((util_log_sink == NULL) || (format == NULL)) {
        return;
    }

    va_start(arguments, format);
    (void)vsnprintf(util_log_line, sizeof(util_log_line), format, arguments);
    va_end(arguments);
    util_log_line[sizeof(util_log_line) - 1U] = '\0';

    util_log_sink(level, (tag == NULL) ? "" : tag, util_log_line);
}
