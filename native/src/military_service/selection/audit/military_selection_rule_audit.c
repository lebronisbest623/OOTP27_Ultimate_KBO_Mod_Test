#include "military_selection_rule_audit.h"

#include "../../../core/logging/rule_audit.h"

void kbo_military_selection_audit_flow(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t entry_year,
    uint32_t sang_id,
    int active_before,
    int slots,
    int queued,
    int considered,
    int routed,
    int refreshed,
    int seeded,
    int returned,
    int news_created,
    uint32_t vector_offset)
{
    kbo_rule_audit_emitf(
        rule,
        decision,
        reason,
        source,
        "\"year\":%u,\"service_team_id\":%u,\"active_before\":%d,\"slots\":%d,"
        "\"queued\":%d,\"considered\":%d,\"routed\":%d,\"refreshed\":%d,"
        "\"seeded\":%d,\"returned\":%d,\"news\":%d,\"vector_offset\":%u",
        entry_year,
        sang_id,
        active_before,
        slots,
        queued,
        considered,
        routed,
        refreshed,
        seeded,
        returned,
        news_created,
        vector_offset);
}

void kbo_military_selection_audit_player(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t entry_year,
    uint32_t player_id,
    uint32_t original_team_id,
    uint32_t service_team_id,
    int score,
    int called_pre_change,
    int called_register,
    int called_attach)
{
    kbo_rule_audit_emitf(
        "military.selection.player",
        decision,
        reason,
        source,
        "\"year\":%u,\"player_id\":%u,\"original_team_id\":%u,"
        "\"service_team_id\":%u,\"score\":%d,\"pre_change\":%d,"
        "\"register\":%d,\"attach\":%d",
        entry_year,
        player_id,
        original_team_id,
        service_team_id,
        score,
        called_pre_change,
        called_register,
        called_attach);
}
