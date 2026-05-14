#include "foreign_priority_event_audit.h"

#include "../../../core/logging/rule_audit.h"

void kbo_audit_foreign_priority_schedule(
    const char* decision,
    const char* reason,
    const char* source,
    const KboForeignPriorityEventAudit* audit)
{
    KboForeignPriorityEventAudit empty = {0};
    const KboForeignPriorityEventAudit* a = audit != 0 ? audit : &empty;
    kbo_rule_audit_emitf(
        "custom_event.foreign_priority.schedule",
        decision,
        reason,
        source,
        "\"date\":%u,\"league_id\":%u,\"anchor_date\":%u,\"open_date\":%u,"
        "\"close_date\":%u,\"fa_declaration_date\":%u,\"military_selection_date\":%u,"
        "\"created_open\":%d,\"created_close\":%d,\"created_fa_declaration\":%d,"
        "\"created_military\":%d,\"ready\":%d",
        a->today,
        a->league_id,
        a->anchor_date,
        a->open_date,
        a->close_date,
        a->fa_declaration_date,
        a->military_selection_date,
        a->created_open,
        a->created_close,
        a->created_fa_declaration,
        a->created_military,
        a->ready);
}
