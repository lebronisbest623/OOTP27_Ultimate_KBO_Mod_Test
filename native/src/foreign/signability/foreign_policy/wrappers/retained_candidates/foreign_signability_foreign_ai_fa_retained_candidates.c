#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include "../../../../../core/logging/core_log.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../common/dates/foreign_waiver_date.h"
#include "../../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../rights/query/foreign_waiver_rights_query.h"
#include "../candidate_array/foreign_signability_ai_fa_candidate_array.h"
#include "foreign_signability_foreign_ai_fa_retained_candidates.h"

#define KBO_AI_FA_STATUS_FORCED_RETENTION_MAX 16

static int kbo_ai_fa_status_retention_recently_attempted(uintptr_t frame_ptr, uintptr_t candidate_array)
{
    static uintptr_t last_frame_ptr = 0;
    static uintptr_t last_candidate_array = 0;
    static DWORD last_attempt_tick = 0u;

    DWORD now = GetTickCount();
    if (last_frame_ptr == frame_ptr
            && last_candidate_array == candidate_array
            && last_attempt_tick != 0u
            && now - last_attempt_tick < 1000u) {
        return 1;
    }

    last_frame_ptr = frame_ptr;
    last_candidate_array = candidate_array;
    last_attempt_tick = now;
    return 0;
}

static int kbo_ai_fa_status_retention_priority_enabled(void)
{
    return read_kbo_localappdata_flag_file("enable_foreign_ai_roster_management.txt") ? 1 : 0;
}

static int kbo_ai_fa_status_retained_player_can_enter_market(
    uint8_t* player,
    uint32_t expected_player_id,
    uint32_t requester_team_id,
    uint32_t today)
{
    if (player == NULL
            || requester_team_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != expected_player_id
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }
    if (!kbo_has_active_foreign_waiver_right(requester_team_id, expected_player_id, today)) {
        return 0;
    }
    if (kbo_player_current_assignment_matches_team_or_affiliate(player, requester_team_id)) {
        return 0;
    }

    int32_t score = kbo_foreign_waiver_value_score(player);
    int32_t threshold = kbo_get_foreign_waiver_value_threshold_for_player(player);
    return score >= threshold;
}

int32_t kbo_ai_fa_status_force_retained_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index)
{
    static volatile LONG retention_skip_log_count = 0;
    if (candidate_array == 0
            || insert_index < 0
            || requester_team_id == 0u
            || !kbo_ai_fa_status_retention_priority_enabled()) {
        LONG slot = InterlockedIncrement(&retention_skip_log_count);
        if (slot <= 80) {
            append_logf(
                "foreign retention priority: skip_enter requester_team=%u candidate_array=%p index=%d enabled=%d",
                requester_team_id,
                (void*)candidate_array,
                insert_index,
                kbo_ai_fa_status_retention_priority_enabled());
        }
        return insert_index;
    }
    if (kbo_ai_fa_status_retention_recently_attempted(frame_ptr, candidate_array)) {
        return insert_index;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        LONG slot = InterlockedIncrement(&retention_skip_log_count);
        if (slot <= 80) {
            append_logf(
                "foreign retention priority: skip_no_date team=%u index=%d",
                requester_team_id,
                insert_index);
        }
        return insert_index;
    }

    kbo_prune_expired_foreign_waiver_rights(today);
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    uint32_t player_ids[KBO_AI_FA_STATUS_FORCED_RETENTION_MAX] = {0};
    int player_count = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count && player_count < KBO_AI_FA_STATUS_FORCED_RETENTION_MAX; i++) {
        const KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == requester_team_id && kbo_is_foreign_waiver_right_active(rec, today)) {
            player_ids[player_count++] = rec->player_id;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (player_count == 0) {
        LONG slot = InterlockedIncrement(&retention_skip_log_count);
        if (slot <= 80) {
            append_logf(
                "foreign retention priority: skip_no_rights team=%u today=%u index=%d",
                requester_team_id,
                today,
                insert_index);
        }
        return insert_index;
    }

    int considered = 0;
    int protectable = 0;
    int already_present = 0;
    for (int i = 0; i < player_count; i++) {
        uint32_t retained_player_id = player_ids[i];
        uint8_t* retained_player = kbo_find_player_by_id(retained_player_id, NULL, NULL);
        considered++;
        if (!kbo_ai_fa_status_retained_player_can_enter_market(
                retained_player,
                retained_player_id,
                requester_team_id,
                today)) {
            continue;
        }
        protectable++;

        uintptr_t retained_player_ptr = (uintptr_t)retained_player;
        if (kbo_ai_fa_status_candidate_array_contains(candidate_array, insert_index, retained_player_ptr)) {
            already_present++;
            continue;
        }

        int32_t before_index = insert_index;
        insert_index = kbo_ai_fa_status_insert_candidate_ptr(
            frame_ptr,
            candidate_array,
            insert_index,
            retained_player_ptr);
        if (insert_index != before_index) {
            static volatile LONG retention_force_log_count = 0;
            LONG slot = InterlockedIncrement(&retention_force_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign retention priority: force_retained_candidate team=%u player=%u score=%d index=%d next=%d today=%u",
                    requester_team_id,
                    retained_player_id,
                    kbo_foreign_waiver_value_score(retained_player),
                    before_index,
                    insert_index,
                    today);
            }
        }
    }

    if (protectable == 0 || protectable == already_present) {
        LONG slot = InterlockedIncrement(&retention_skip_log_count);
        if (slot <= 80) {
            append_logf(
                "foreign retention priority: no_force team=%u today=%u rights=%d considered=%d protectable=%d already_present=%d index=%d",
                requester_team_id,
                today,
                player_count,
                considered,
                protectable,
                already_present,
                insert_index);
        }
    }

    return insert_index;
}
