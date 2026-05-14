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
    kbo_rule_audit_emitf(
        "fa_salary_snapshot.capture",
        "skip",
        reason != NULL ? reason : "unknown",
        source != NULL ? source : "fa_salary_snapshot",
        "\"date\":%u,\"season\":%u,\"opening_day\":%u,\"league_id\":%u,\"player_count\":%d",
        date,
        season,
        opening_day,
        league_id,
        player_count);
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
    kbo_rule_audit_emitf(
        "fa_salary_snapshot.capture",
        decision != NULL ? decision : "record",
        reason != NULL ? reason : "capture_processed",
        source != NULL ? source : "fa_salary_snapshot",
        "\"date\":%u,\"season\":%u,\"opening_day\":%u,\"league_id\":%u,\"scanned\":%d,"
        "\"rows\":%d,\"salary_rows\":%d,\"zero_salary_rows\":%d",
        date,
        season,
        opening_day,
        league_id,
        scanned,
        row_count,
        salary_rows,
        zero_salary_rows);
}
