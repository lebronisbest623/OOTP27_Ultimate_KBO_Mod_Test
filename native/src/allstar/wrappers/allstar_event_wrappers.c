/* All-Star event and voting preparation wrappers. */

#include "../../bootstrap/abi/hook_entrypoints.h"

#include <stdint.h>
#include <windows.h>

#include "../flags/allstar_flags.h"
#include "../allstar_league_context/allstar_league_context.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/perf_probe.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"

__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr)
{
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    enable_kbo_allstar_flags(league_ptr, "make_allstar_game_events");
    kbo_perf_probe_record(
        "allstar_events_prepare",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
}

__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr)
{
    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (!memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))) {
        return;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint32_t old_team_a = kbo_allstar_read_u32(league, layout.team_a_offset);
    uint32_t old_team_b = kbo_allstar_read_u32(league, layout.team_b_offset);
    int setup_called = 0;

    enable_kbo_allstar_flags(league_ptr, "allstar_voting_begin");
    if (allstar_team_setup_ptr != 0 && memory_range_readable((void*)allstar_team_setup_ptr, 16u)) {
        OotpAllstarTeamSetupFn setup_allstar_teams = (OotpAllstarTeamSetupFn)allstar_team_setup_ptr;
        setup_allstar_teams(league_ptr);
        setup_called = 1;
        patch_kbo_allstar_team_names_for_league_id(league_id != 0u ? league_id : fallback_league_id, "allstar_voting_begin_setup");
        patch_kbo_allstar_team_names_for_known_exhibition_teams("allstar_voting_begin_setup");
    }

    LONG log_index = InterlockedIncrement(&g_allstar_voting_prepare_log_count);
    if (log_index <= 20) {
        append_logf(
            "prepared KBO all-star voting begin league=%p league_id=%u/%u year=%u game=%u->%u teams=%u/%u->%u/%u setup=%p called=%d",
            league,
            league_id,
            fallback_league_id,
            league_year,
            old_allstar_game,
            league[layout.game_flag_offset],
            old_team_a,
            old_team_b,
            kbo_allstar_read_u32(league, layout.team_a_offset),
            kbo_allstar_read_u32(league, layout.team_b_offset),
            (void*)allstar_team_setup_ptr,
            setup_called);
    }
}
