#include "rule_audit.h"

#include "core_log.h"

#include <stdarg.h>
#include <stdio.h>

static void kbo_rule_audit_append_text(
    char* out,
    size_t out_size,
    size_t* pos,
    const char* text)
{
    if (out == NULL || out_size == 0u || pos == NULL || *pos >= out_size) {
        return;
    }

    int written = snprintf(out + *pos, out_size - *pos, "%s", text != NULL ? text : "");
    if (written < 0) {
        return;
    }
    if ((size_t)written >= out_size - *pos) {
        *pos = out_size - 1u;
    } else {
        *pos += (size_t)written;
    }
}

static void kbo_rule_audit_escape_json(
    char* out,
    size_t out_size,
    const char* value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    size_t pos = 0u;
    for (const unsigned char* p = (const unsigned char*)value; *p != '\0' && pos + 1u < out_size; p++) {
        switch (*p) {
        case '"':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\\"");
            break;
        case '\\':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\\\");
            break;
        case '\b':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\b");
            break;
        case '\f':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\f");
            break;
        case '\n':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\n");
            break;
        case '\r':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\r");
            break;
        case '\t':
            kbo_rule_audit_append_text(out, out_size, &pos, "\\t");
            break;
        default:
            if (*p < 0x20u) {
                char escaped[8] = {0};
                snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned)*p);
                kbo_rule_audit_append_text(out, out_size, &pos, escaped);
            } else {
                out[pos++] = (char)*p;
                out[pos] = '\0';
            }
            break;
        }
    }
    out[out_size - 1u] = '\0';
}

void kbo_rule_audit_emit(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const char* fields_json)
{
    char rule_json[192] = {0};
    char decision_json[96] = {0};
    char reason_json[192] = {0};
    char source_json[192] = {0};
    kbo_rule_audit_escape_json(rule_json, sizeof(rule_json), rule);
    kbo_rule_audit_escape_json(decision_json, sizeof(decision_json), decision);
    kbo_rule_audit_escape_json(reason_json, sizeof(reason_json), reason);
    kbo_rule_audit_escape_json(source_json, sizeof(source_json), source);

    char message[3072] = {0};
    if (fields_json != NULL && fields_json[0] != '\0') {
        snprintf(
            message,
            sizeof(message),
            "KBO_RULE_AUDIT {\"rule\":\"%s\",\"decision\":\"%s\",\"reason\":\"%s\",\"source\":\"%s\",%s}",
            rule_json,
            decision_json,
            reason_json,
            source_json,
            fields_json);
    } else {
        snprintf(
            message,
            sizeof(message),
            "KBO_RULE_AUDIT {\"rule\":\"%s\",\"decision\":\"%s\",\"reason\":\"%s\",\"source\":\"%s\"}",
            rule_json,
            decision_json,
            reason_json,
            source_json);
    }
    append_log_line(message);
}

void kbo_rule_audit_emitf(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const char* fields_format,
    ...)
{
    char fields[2048] = {0};
    if (fields_format != NULL && fields_format[0] != '\0') {
        va_list args;
        va_start(args, fields_format);
        vsnprintf(fields, sizeof(fields), fields_format, args);
        va_end(args);
    }

    kbo_rule_audit_emit(rule, decision, reason, source, fields);
}
