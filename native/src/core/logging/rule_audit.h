#ifndef KBO_CORE_LOGGING_RULE_AUDIT_H_
#define KBO_CORE_LOGGING_RULE_AUDIT_H_

void kbo_rule_audit_emit(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const char* fields_json);

void kbo_rule_audit_emitf(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const char* fields_format,
    ...);

#endif
