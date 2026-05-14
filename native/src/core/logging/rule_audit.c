#include "rule_audit.h"

#include <string.h>

#define KBO_RULE_AUDIT_FILE_NAME "rule_audit.ndjson"

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
    kbo_log_fields_merge(&combined, fields);

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
        KBO_LOG_SINK_GLOBAL | KBO_LOG_SINK_SAVE_SCOPED,
        KBO_LOG_RULE_AUDIT_MAX_BYTES,
        KBO_LOG_DEFAULT_ARCHIVES);
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
