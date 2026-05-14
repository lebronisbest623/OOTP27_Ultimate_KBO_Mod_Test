#include "core_log.h"

#include "event/log_event.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define KBO_LOG_RUNTIME_MESSAGE_BYTES 3072u
#define KBO_LOG_SOURCE_FILE_BYTES 256u
#define KBO_LOG_SOURCE_DOMAIN_BYTES 64u

static const char* kbo_log_relative_source_start(const char* file)
{
    if (file == NULL || file[0] == '\0') {
        return "";
    }

    static const char* const markers[] = {
        "native/src/",
        "native\\src\\",
        "src/",
        "src\\"
    };
    for (size_t i = 0u; i < sizeof(markers) / sizeof(markers[0]); i++) {
        const char* found = strstr(file, markers[i]);
        if (found != NULL) {
            return found + strlen(markers[i]);
        }
    }
    return file;
}

static int kbo_log_source_domain_char(char ch)
{
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9')
        || ch == '_';
}

static void kbo_log_copy_source_file(const char* file, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    const char* relative = kbo_log_relative_source_start(file);
    if (relative[0] == '\0') {
        return;
    }

    size_t pos = 0u;
    while (relative[pos] != '\0' && pos + 1u < out_size) {
        out[pos] = relative[pos] == '\\' ? '/' : relative[pos];
        pos++;
    }
    out[pos] = '\0';
}

static void kbo_log_copy_source_domain(const char* file, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    const char* relative = kbo_log_relative_source_start(file);
    if (relative[0] == '\0') {
        snprintf(out, out_size, "runtime");
        return;
    }

    size_t pos = 0u;
    for (const char* p = relative; *p != '\0' && *p != '/' && *p != '\\' && *p != '.'; p++) {
        if (pos + 1u >= out_size) {
            break;
        }
        out[pos++] = kbo_log_source_domain_char(*p) ? *p : '_';
    }
    if (pos == 0u) {
        snprintf(out, out_size, "runtime");
        return;
    }
    out[pos] = '\0';
}

void kbo_log_runtime_line_at(const char* file, int line, const char* message)
{
    char domain[KBO_LOG_SOURCE_DOMAIN_BYTES] = {0};
    char source_file[KBO_LOG_SOURCE_FILE_BYTES] = {0};
    kbo_log_copy_source_domain(file, domain, sizeof(domain));
    kbo_log_copy_source_file(file, source_file, sizeof(source_file));

    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_str(&fields, "message", message != NULL ? message : "");
    if (line > 0) {
        kbo_log_field_i32(&fields, "source_line", line);
    }
    kbo_log_runtime_event(
        KBO_LOG_LEVEL_INFO,
        domain,
        "message",
        NULL,
        NULL,
        source_file[0] != '\0' ? source_file : "runtime",
        &fields);
}

void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    char message[KBO_LOG_RUNTIME_MESSAGE_BYTES] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format != NULL ? format : "", args);
    va_end(args);
    kbo_log_runtime_line_at(file, line, message);
}
