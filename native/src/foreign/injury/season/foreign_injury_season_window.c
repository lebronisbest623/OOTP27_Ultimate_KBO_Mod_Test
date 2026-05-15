#include "../internal/foreign_injury_internal.h"
#include "../../../core/season/phase/season_phase.h"

static LONG g_kbo_foreign_injury_season_window_block_log_count = 0;

int kbo_foreign_injury_replacement_in_season_window(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context)
{
    if (league_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    KboSeasonPhaseInfo phase_info;
    memset(&phase_info, 0, sizeof(phase_info));
    if (!kbo_season_phase_resolve(league_id, today_yyyymmdd, 0u, &phase_info)) {
        LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_season_window_block_log_count);
        if (log_slot <= 40 || (log_slot % 200) == 0) {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.season_window",
                "block",
                "season_phase_unavailable",
                source,
                &audit_fields);
            kbo_log_runtimef(
                "foreign injury replacement: blocked outside in-season context=%s source=%s league=%u date=%u reason=season_phase_unavailable",
                context != NULL ? context : "",
                source != NULL ? source : "",
                league_id,
                today_yyyymmdd);
        }
        return 0;
    }

    if (kbo_foreign_injury_replacement_phase_allows_signing(phase_info.effective_phase)) {
        return 1;
    }

    LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_season_window_block_log_count);
    if (log_slot <= 40 || (log_slot % 200) == 0) {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_u32(&audit_fields, "effective_phase", (uint32_t)phase_info.effective_phase);
        kbo_log_field_u32(&audit_fields, "raw_phase", (uint32_t)phase_info.raw_phase);
        kbo_log_field_u32(&audit_fields, "opening_day", phase_info.opening_day);
        kbo_rule_audit_emit_fields(
            "foreign_injury.replacement.season_window",
            "block",
            kbo_season_phase_label(phase_info.effective_phase),
            source,
            &audit_fields);
        kbo_log_runtimef(
            "foreign injury replacement: blocked outside in-season context=%s source=%s league=%u date=%u phase=%s raw=%u opening_day=%u",
            context != NULL ? context : "",
            source != NULL ? source : "",
            league_id,
            today_yyyymmdd,
            kbo_season_phase_label(phase_info.effective_phase),
            (uint32_t)phase_info.raw_phase,
            phase_info.opening_day);
    }
    return 0;
}
