#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../fa_compensation/history/fa_compensation_history.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../fa_requalification.h"
#include <stdint.h>

#include "../fa_requalification_internal.h"
/* ---- KBO FA requalification control ---- */

#ifndef KBO_FA_REQUALIFICATION_TYPES_DEFINED
#define KBO_FA_REQUALIFICATION_TYPES_DEFINED

#define KBO_FA_REQUALIFICATION_MAX 4096

typedef struct KboFaRequalificationRecord {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t last_fa_year;
    uint32_t fa_count;
} KboFaRequalificationRecord;

#endif

LONG g_kbo_fa_requalification_thread_started = 0;
LONG g_kbo_fa_requalification_no_date_log_count = 0;
LONG g_kbo_fa_requalification_no_records_log_count = 0;
LONG g_kbo_fa_requalification_skip_log_count = 0;
LONG g_kbo_fa_requalification_hook_skip_log_count = 0;
volatile LONG g_kbo_fa_requalification_records_lock = 0;
uint32_t g_kbo_fa_requalification_last_no_records_date = 0;










uint8_t* kbo_find_fa_requalification_player_by_id(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}


__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    if (!kbo_fix_enabled()) {
        return 1;
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player player_ptr=%p team_ptr=%p", (void*)player_ptr, (void*)team_ptr);
        }
        return 1;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || player_id > 1000000u) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player_id player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return 1;
    }

    int is_kbo_team = kbo_fa_requalification_team_ptr_is_kbo(team_ptr, &team_id, &league_id);

    if (kbo_team_id_is_military_service_team(team_id)) {
        static volatile LONG military_fa_signing_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_fa_signing_block_log_count);
        if (slot <= 200) {
            append_logf(
                "military service team FA signing blocked player=%u team=%u league=%u",
                player_id,
                team_id,
                league_id);
        }
        return 0;
    }

    if (!is_kbo_team) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=non_kbo_team player=%u team_ptr=%p team=%u league=%u", player_id, (void*)team_ptr, team_id, league_id);
        }
        return 1;
    }

    if (kbo_custom_foreign_policy_enabled()
            && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = 0u;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_final_signing(
            team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            uint32_t today = 0u;
            if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
                kbo_get_current_yyyymmdd(&today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, team_id, today);
            static volatile LONG final_block_log_count = 0;
            LONG slot = InterlockedIncrement(&final_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "custom foreign policy FA signing blocked player=%u team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    kbo_foreign_injury_slot_label(slot_type),
                    injured_player_id,
                    today);
            }
            return 0;
        }
    }

    return 1;
}

__declspec(noinline) void ootp_kbo_fa_signing_success_post_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    if (!kbo_fix_enabled() || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    if (!kbo_fa_requalification_team_ptr_is_kbo(team_ptr, &team_id, &league_id)) {
        return;
    }
    if (kbo_team_id_is_military_service_team(team_id)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET)
        : 0u;

    static volatile LONG post_log_count = 0;
    LONG slot = InterlockedIncrement(&post_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO FA signing success post player=%u team=%u league=%u current_team=%u active_team=%u original_team=%u",
            player_id,
            team_id,
            league_id,
            current_team_id,
            active_team_id,
            original_team_id);
    }

    kbo_record_fa_compensation_signing(player_ptr, team_id, league_id, "fa_signing_success_post");

    if (!read_kbo_localappdata_flag_file("enable_fa_requalification.txt")) {
        return;
    }

    uint32_t signing_year = kbo_find_league_year_from_id(league_id);
    if (signing_year < 1982u || signing_year > 2200u) {
        uint32_t today = 0u;
        if (kbo_get_current_yyyymmdd(&today)) {
            signing_year = today / 10000u;
        }
    }
    if (signing_year < 1982u || signing_year > 2200u) {
        LONG skip_slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (skip_slot <= 20) {
            append_logf("KBO FA signing success post skipped reason=no_signing_year player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return;
    }

    kbo_record_fa_requalification_signing(player_id, team_id, signing_year, "fa_signing_success_post");
}






