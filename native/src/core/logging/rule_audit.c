#include "rule_audit.h"

#include "../core_flags/api/flags_api.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define KBO_RULE_AUDIT_FILE_NAME "rule_audit.ndjson"
#define KBO_RULE_AUDIT_MIRROR_FLAG_FILE "enable_rule_audit_kbofix_mirror.txt"

static KboLogLevel kbo_rule_audit_level_for_decision(const char* decision)
{
    if (decision == NULL) {
        return KBO_LOG_LEVEL_INFO;
    }
    if (_stricmp(decision, "fail") == 0 || _stricmp(decision, "error") == 0) {
        return KBO_LOG_LEVEL_ERROR;
    }
    if (_stricmp(decision, "defer") == 0 || _stricmp(decision, "fallback") == 0) {
        return KBO_LOG_LEVEL_WARN;
    }
    return KBO_LOG_LEVEL_INFO;
}

static void kbo_rule_audit_emit_combined(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const KboLogFields* fields)
{
    KboLogFields combined;
    kbo_log_fields_init(&combined);
    kbo_log_field_str(&combined, "rule", rule != NULL ? rule : "");
    if (fields != NULL && fields->json[0] != '\0') {
        kbo_log_field_raw_json(&combined, fields->json);
        if (fields->truncated) {
            combined.truncated = 1;
        }
    }

    unsigned sink_flags = KBO_LOG_SINK_GLOBAL | KBO_LOG_SINK_SAVE_SCOPED | KBO_LOG_SINK_FALLBACK_MIRROR_CORE;
    if (read_kbo_localappdata_flag_file(KBO_RULE_AUDIT_MIRROR_FLAG_FILE)) {
        sink_flags |= KBO_LOG_SINK_MIRROR_CORE;
    }

    kbo_log_event_emit(
        KBO_RULE_AUDIT_FILE_NAME,
        "rule_audit",
        kbo_rule_audit_level_for_decision(decision),
        rule != NULL && rule[0] != '\0' ? rule : "unknown_rule",
        "rule_decision",
        decision,
        reason,
        source,
        &combined,
        sink_flags,
        KBO_LOG_RULE_AUDIT_MAX_BYTES,
        KBO_LOG_DEFAULT_ARCHIVES,
        "KBO_RULE_AUDIT");
}

void kbo_rule_audit_emit(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const char* fields_json)
{
    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_raw_json(&fields, fields_json);
    kbo_rule_audit_emit_combined(rule, decision, reason, source, &fields);
}

void kbo_rule_audit_emit_fields(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const KboLogFields* fields)
{
    kbo_rule_audit_emit_combined(rule, decision, reason, source, fields);
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
