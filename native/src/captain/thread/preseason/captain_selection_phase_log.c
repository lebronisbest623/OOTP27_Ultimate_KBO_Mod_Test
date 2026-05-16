
#include "../../internal/captain_selection_internal.h"

static const char* kbo_captain_phase_label(uint8_t phase)
{
    return kbo_season_phase_label(phase);
}
void kbo_captain_log_phase_observed(
    const char* source,
    uint32_t date,
    uint32_t league_id,
    uintptr_t league_ptr,
    uint32_t league_season,
    uint32_t effective_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason)
{
    static uintptr_t last_league_ptr = 0u;
    static uint32_t last_date = 0xffffffffu;
    static uint32_t last_league_id = 0xffffffffu;
    static uint32_t last_league_season = 0xffffffffu;
    static uint32_t last_effective_season = 0xffffffffu;
    static uint8_t last_phase = 0xffu;
    static int last_csv_exists = -1;
    static int last_calendar_recovery = -1;
    static int last_calendar_preseason = -1;
    if (league_ptr == last_league_ptr
            && date == last_date
            && league_id == last_league_id
            && league_season == last_league_season
            && effective_season == last_effective_season
            && phase == last_phase
            && csv_exists == last_csv_exists
            && calendar_recovery == last_calendar_recovery
            && calendar_preseason == last_calendar_preseason) {
        return;
    }
    kbo_log_runtimef(
        "KBO captain phase observed source=%s date=%u league_id=%u league=%p league_season=%u effective_season=%u phase=%u label=%s csv_exists=%d calendar_recovery=%d calendar_preseason=%d",
        source != NULL ? source : "",
        date,
        league_id,
        (void*)league_ptr,
        league_season,
        effective_season,
        (unsigned)phase,
        kbo_captain_phase_label(phase),
        csv_exists,
        calendar_recovery,
        calendar_preseason);

    last_league_ptr = league_ptr;
    last_date = date;
    last_league_id = league_id;
    last_league_season = league_season;
    last_effective_season = effective_season;
    last_phase = phase;
    last_csv_exists = csv_exists;
    last_calendar_recovery = calendar_recovery;
    last_calendar_preseason = calendar_preseason;
}

