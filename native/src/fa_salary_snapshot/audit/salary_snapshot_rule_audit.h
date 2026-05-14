#ifndef KBO_FA_SALARY_SNAPSHOT_AUDIT_SALARY_SNAPSHOT_RULE_AUDIT_H_
#define KBO_FA_SALARY_SNAPSHOT_AUDIT_SALARY_SNAPSHOT_RULE_AUDIT_H_

#include <stdint.h>

void kbo_audit_fa_salary_snapshot_capture_skip(
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    int player_count);

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
    int zero_salary_rows);

#endif
