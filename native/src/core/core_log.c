#include "core_log.h"

#include "../bootstrap/profiler.h"

#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

void append_log_line(const char* message)
{
    LARGE_INTEGER profile_start = {0};
    int profile_active = kbo_profiler_begin(&profile_start);

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        if (profile_active) {
            kbo_profiler_end("log.append_line.env_unavailable", &profile_start);
        }
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s\\OOTP-KBO", local_app_data);
    CreateDirectoryA(dir, NULL);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\kbofix.log", dir);

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (profile_active) {
            kbo_profiler_end("log.append_line.open_failed", &profile_start);
        }
        return;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);

    char line[4096] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu %s\r\n",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds,
        GetCurrentProcessId(),
        message);

    if (len > 0) {
        DWORD written = 0;
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }

    CloseHandle(file);
    if (profile_active) {
        kbo_profiler_end("log.append_line", &profile_start);
    }
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
