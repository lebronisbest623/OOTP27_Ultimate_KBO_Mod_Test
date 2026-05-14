#ifndef KBO_CAPTAIN_AUDIT_CAPTAIN_RULE_AUDIT_H_
#define KBO_CAPTAIN_AUDIT_CAPTAIN_RULE_AUDIT_H_

#include <stdint.h>

void kbo_captain_audit_preseason_selection(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    int row_count,
    int selected_count);

void kbo_captain_audit_preseason_bootstrap(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase);

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
    int summary_rows);

#endif
