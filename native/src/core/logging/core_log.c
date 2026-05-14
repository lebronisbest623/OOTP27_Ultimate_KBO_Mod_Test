#include "core_log.h"

#include "event/log_event.h"

#include <stdarg.h>
#include <stdio.h>

void append_log_line(const char* message)
{
    kbo_log_runtime_message(KBO_LOG_LEVEL_INFO, "legacy", "message", message);
}

void append_logf(const char* format, ...)
{
    char message[3072] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    append_log_line(message);
}
