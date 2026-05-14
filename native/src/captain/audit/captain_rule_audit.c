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
    kbo_rule_audit_emitf(
        "captain.preseason.selection",
        decision,
        reason,
        source,
        "\"date\":%u,\"season\":%u,\"league_id\":%u,\"rows\":%d,\"selected\":%d",
        date,
        season,
        league_id,
        row_count,
        selected_count);
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
    kbo_rule_audit_emitf(
        "captain.preseason.bootstrap",
        decision,
        reason,
        source,
        "\"date\":%u,\"season\":%u,\"league_id\":%u,\"phase\":%u",
        date,
        season,
        league_id,
        (unsigned)phase);
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
    kbo_rule_audit_emitf(
        "captain.seed_startup",
        decision,
        reason,
        source,
        "\"date\":%u,\"season\":%u,\"league_id\":%u,\"startup_window\":%d,"
        "\"seed_available\":%d,\"csv_exists\":%d,\"summary_rows\":%d",
        date,
        season,
        league_id,
        startup_window,
        seed_available,
        csv_exists,
        summary_rows);
}
