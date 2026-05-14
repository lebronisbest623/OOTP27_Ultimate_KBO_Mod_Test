#include "salary_snapshot_rule_audit.h"

#include "../../core/logging/rule_audit.h"

void kbo_audit_fa_salary_snapshot_capture_skip(
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    int player_count)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", date);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "opening_day", opening_day);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "player_count", player_count);
        kbo_rule_audit_emit_fields(
            "fa_salary_snapshot.capture",
            "skip",
            reason != NULL ? reason : "unknown",
            source != NULL ? source : "fa_salary_snapshot",
            &audit_fields);
    } while (0);
}

void kbo_audit_fa_salary_snapshot_capture_result(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    int scanned,
    int row_count,
    int salary_rows,
    int zero_salary_rows)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", date);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "opening_day", opening_day);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "scanned", scanned);
        kbo_log_field_i32(&audit_fields, "rows", row_count);
        kbo_log_field_i32(&audit_fields, "salary_rows", salary_rows);
        kbo_log_field_i32(&audit_fields, "zero_salary_rows", zero_salary_rows);
        kbo_rule_audit_emit_fields(
            "fa_salary_snapshot.capture",
            decision != NULL ? decision : "record",
            reason != NULL ? reason : "capture_processed",
            source != NULL ? source : "fa_salary_snapshot",
            &audit_fields);
    } while (0);
}
