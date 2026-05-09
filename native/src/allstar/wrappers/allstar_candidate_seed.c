/* All-Star candidate team seeding wrapper. */

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/profiling/perf_probe.h"

#include <stdint.h>
#include <windows.h>

#include "../flags/allstar_flags.h"
#include "../allstar_league_context/allstar_league_context.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"

__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr)
{
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    kbo_perf_probe_record(
        "allstar_candidate_team_split_enter",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        0);

    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (left_team_vector == NULL || right_team_vector == NULL || vector_push_back_ptr == 0) {
        append_logf("KBO allstar candidate seed: return=0 reason=null_vectors left=%p right=%p push=%p", left_team_vector, right_team_vector, (void*)vector_push_back_ptr);
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    uint32_t league_year = kbo_find_current_kbo_league_year();
    if (league_year == 0u && league_ptr != 0
            && memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        uint32_t probe = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        if (probe >= 1982u && probe <= 2200u) {
            league_year = probe;
        }
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        append_log_line("KBO allstar candidate seed: return=0 reason=no_global_db");
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000 || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        append_logf("KBO allstar candidate seed: return=0 reason=no_team_vector league_id=%u year=%u", league_id, league_year);
        return 0;
    }

    ensure_kbo_allstar_team_ids(league_ptr, "allstar_candidate_team_split");

    uintptr_t eligible_team_ptrs[128] = {0};
    uint32_t eligible_team_ids[128] = {0};
    uint8_t eligible_sides[128] = {0};
    int eligible_count = 0;
    int side_rule_count = 0;
    int unknown_side_count = 0;
    uint32_t left_ids[64] = {0};
    uint32_t right_ids[64] = {0};
    int left_count = 0;
    int right_count = 0;
    OotpVectorPushBack push_back = (OotpVectorPushBack)vector_push_back_ptr;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0 || team[OOTP27_KBO_TEAM_DELETED_OFFSET + 1u] != 0) {
            continue;
        }
        if (!kbo_allstar_team_matches_league_ids(team, league_id, 0u)) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0 || team_id > (uint32_t)team_count) {
            continue;
        }
        if (eligible_count >= (int)(sizeof(eligible_team_ptrs) / sizeof(eligible_team_ptrs[0]))) {
            continue;
        }

        uint8_t side = kbo_allstar_side_for_team(team, league_year);
        eligible_team_ptrs[eligible_count] = team_ptr;
        eligible_team_ids[eligible_count] = team_id;
        eligible_sides[eligible_count] = side;
        eligible_count++;
        if (side == 1 || side == 2) {
            side_rule_count++;
        } else {
            unknown_side_count++;
        }
    }

    for (int i = 0; i < eligible_count; i++) {
        if (eligible_sides[i] == 1) {
            left_count++;
        } else if (eligible_sides[i] == 2) {
            right_count++;
        }
    }

    int used_csv_split = left_count > 0 && right_count > 0 && unknown_side_count == 0;
    left_count = 0;
    right_count = 0;

    for (int i = 0; i < eligible_count; i++) {
        int send_left = 0;
        if (used_csv_split) {
            if (eligible_sides[i] == 0) {
                continue;
            }
            send_left = eligible_sides[i] == 1;
        } else {
            send_left = ((left_count + right_count) & 1) == 0;
        }

        if (send_left) {
            push_back(left_team_vector, (void*)eligible_team_ptrs[i]);
            if (left_count < (int)(sizeof(left_ids) / sizeof(left_ids[0]))) {
                left_ids[left_count] = eligible_team_ids[i];
            }
            left_count++;
        } else {
            push_back(right_team_vector, (void*)eligible_team_ptrs[i]);
            if (right_count < (int)(sizeof(right_ids) / sizeof(right_ids[0]))) {
                right_ids[right_count] = eligible_team_ids[i];
            }
            right_count++;
        }
    }

    if (left_count <= 0 || right_count <= 0) {
        append_logf("KBO allstar candidate seed: return=0 reason=empty_side league_id=%u year=%u eligible=%d left=%d right=%d used_csv=%d", league_id, league_year, eligible_count, left_count, right_count, used_csv_split);
        return 0;
    }

    /* Cache the league pointer so kbo_find_allstar_league_ptr can use it, and set
     * game_flag/rules_flag/auto_schedule immediately so the voting ballot shows candidates.
     * The retry thread may have already given up by the time the candidate split fires,
     * so this is the most reliable path to ensure game_flag=1 before voting is checked.
     *
     * enable_kbo_allstar_flags checks kbo_allstar_league_core_plausible, which reads
     * OOTP27_KBO_LEAGUE_YEAR_OFFSET — that offset may be wrong for this build's league
     * struct, causing a silent early return. Write flags directly here as a fallback since
     * league_ptr comes from OOTP itself via R14 at the hook site and is guaranteed valid. */
    if (league_ptr != 0) {
        InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            (PVOID)league_ptr,
            NULL);
        enable_kbo_allstar_flags(league_ptr, "candidate_split");

        KboAllstarLayout asl = kbo_get_allstar_layout();
        uint32_t max_off = asl.game_flag_offset > asl.auto_schedule_offset
            ? asl.game_flag_offset : asl.auto_schedule_offset;
        max_off = max_off > asl.rules_flag_offset ? max_off : asl.rules_flag_offset;
        if (memory_range_readable((void*)(league_ptr + max_off), 1)) {
            uint8_t* lp = (uint8_t*)league_ptr;
            uint8_t old_game  = lp[asl.game_flag_offset];
            uint8_t old_rules = lp[asl.rules_flag_offset];
            uint8_t old_auto  = lp[asl.auto_schedule_offset];
            lp[asl.game_flag_offset]  = 1;
            lp[asl.rules_flag_offset] = 1;
            lp[asl.auto_schedule_offset] = 1;
            if (old_game != 1 || old_rules != 1 || old_auto != 1) {
                append_logf(
                    "KBO candidate split: direct allstar flags league=%p game 0x%x=%u->1 rules 0x%x=%u->1 auto 0x%x=%u->1",
                    (void*)league_ptr,
                    asl.game_flag_offset, (unsigned)old_game,
                    asl.rules_flag_offset, (unsigned)old_rules,
                    asl.auto_schedule_offset, (unsigned)old_auto);
            }
        } else {
            append_logf(
                "KBO candidate split: direct allstar flags skipped league=%p max_off=0x%x not readable",
                (void*)league_ptr, max_off);
        }
    }

    LONG log_index = InterlockedIncrement(&g_allstar_candidate_seed_log_count);
    if (log_index <= 20) {
        append_logf(
            "seeded KBO single-division all-star candidate teams league_id=%u year=%u teams_scanned=%d left_count=%d first_left=%u right_count=%d first_right=%u",
            league_id,
            league_year,
            team_count,
            left_count,
            left_ids[0],
            right_count,
            right_ids[0]);
        append_logf(
            "KBO all-star candidate team split source=allstar_teams.csv league_id=%u eligible=%d rule_sides=%d unknown_sides=%d mode=%s",
            league_id,
            eligible_count,
            side_rule_count,
            unknown_side_count,
            used_csv_split ? "csv" : "fallback_alternating");
    }

    static volatile LONG perf_success_total = 0;
    static volatile LONG perf_success_last = 0;
    static volatile LONG perf_success_ms = 0;
    static volatile LONG perf_success_max = 0;
    static volatile LONG perf_success_tick = 0;
    kbo_perf_probe_record(
        "allstar_candidate_team_split_success",
        &perf_success_total,
        &perf_success_last,
        &perf_success_ms,
        &perf_success_max,
        &perf_success_tick,
        GetTickCount() - perf_start);

    return 1;
}
