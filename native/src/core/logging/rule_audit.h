#ifndef KBO_CORE_LOGGING_RULE_AUDIT_H_
#define KBO_CORE_LOGGING_RULE_AUDIT_H_

#include "event/log_event.h"

void kbo_rule_audit_emit_fields(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const KboLogFields* fields);

#endif
