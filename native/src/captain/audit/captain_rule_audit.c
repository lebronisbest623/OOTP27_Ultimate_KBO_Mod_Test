#include "captain_rule_audit.h"

#include "../../core/logging/rule_audit.h"

void kbo_captain_audit_preseason_selection(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    int row_count,
    int selected_count)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", date);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "rows", row_count);
        kbo_log_field_i32(&audit_fields, "selected", selected_count);
        kbo_rule_audit_emit_fields(
            "captain.preseason.selection",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}

void kbo_captain_audit_preseason_bootstrap(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", date);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_u32(&audit_fields, "phase", (unsigned)phase);
        kbo_rule_audit_emit_fields(
            "captain.preseason.bootstrap",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}

void kbo_captain_audit_seed_startup(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    int startup_window,
    int seed_available,
    int csv_exists,
    int summary_rows)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", date);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "startup_window", startup_window);
        kbo_log_field_i32(&audit_fields, "seed_available", seed_available);
        kbo_log_field_i32(&audit_fields, "csv_exists", csv_exists);
        kbo_log_field_i32(&audit_fields, "summary_rows", summary_rows);
        kbo_rule_audit_emit_fields(
            "captain.seed_startup",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}
