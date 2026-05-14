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
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "year", entry_year);
        kbo_log_field_u32(&audit_fields, "service_team_id", sang_id);
        kbo_log_field_i32(&audit_fields, "active_before", active_before);
        kbo_log_field_i32(&audit_fields, "slots", slots);
        kbo_log_field_i32(&audit_fields, "queued", queued);
        kbo_log_field_i32(&audit_fields, "considered", considered);
        kbo_log_field_i32(&audit_fields, "routed", routed);
        kbo_log_field_i32(&audit_fields, "refreshed", refreshed);
        kbo_log_field_i32(&audit_fields, "seeded", seeded);
        kbo_log_field_i32(&audit_fields, "returned", returned);
        kbo_log_field_i32(&audit_fields, "news", news_created);
        kbo_log_field_u32(&audit_fields, "vector_offset", vector_offset);
        kbo_rule_audit_emit_fields(
            rule,
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
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
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "year", entry_year);
        kbo_log_field_u32(&audit_fields, "player_id", player_id);
        kbo_log_field_u32(&audit_fields, "original_team_id", original_team_id);
        kbo_log_field_u32(&audit_fields, "service_team_id", service_team_id);
        kbo_log_field_i32(&audit_fields, "score", score);
        kbo_log_field_i32(&audit_fields, "pre_change", called_pre_change);
        kbo_log_field_i32(&audit_fields, "register", called_register);
        kbo_log_field_i32(&audit_fields, "attach", called_attach);
        kbo_rule_audit_emit_fields(
            "military.selection.player",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}
