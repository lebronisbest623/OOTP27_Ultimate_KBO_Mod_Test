#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../loans/military_active_loan.h"
#include "../state/military_player_state.h"
#include "../../selection/fa_policy/military_fa_policy.h"
#include "../team_policy/military_service_team_policy.h"
#include "military_team_add_guard.h"

int kbo_military_player_has_active_service_assignment(uintptr_t player_ptr, uint32_t service_team_id)
{
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    int active_index = find_active_kbo_military_loan_index(player_id);
    int32_t days_left = kbo_military_days_left(player);
    uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    return active_index >= 0
        || (military_active != 0u && days_left > 0)
        || (service_team_id != 0u && loan_team_id == service_team_id && days_left > 0);
}

int kbo_military_team_add_player_should_block(uintptr_t team_ptr, uintptr_t player_ptr)
{
    KBO_PROFILE_BEGIN(profile_military_team_add_should_block);
    if (!kbo_fix_enabled()) {
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.disabled");
        return 0;
    }
    if (team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.bad_team");
        return 0;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (!kbo_player_pointer_plausible(player_ptr)) {
        static volatile LONG bad_player_log_count = 0;
        LONG slot = InterlockedIncrement(&bad_player_log_count);
        if (slot <= 40) {
            append_logf(
                "KBO military team-add guard skipped reason=bad_player team=%u player_ptr=%p",
                team_id,
                (void*)player_ptr);
        }
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.bad_player");
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.no_player_id");
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_military_active_lookup);
    int active_index = find_active_kbo_military_loan_index(player_id);
    KBO_PROFILE_END(profile_military_active_lookup, "military.team_add_should_block.active_lookup");
    int32_t days_left = kbo_military_days_left(player);
    uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);

    if (!kbo_team_id_is_military_service_team(team_id)) {
        int serving = active_index >= 0 || (military_active != 0u && days_left > 0);
        if (serving) {
            uint32_t original_team_id = 0u;
            uint32_t original_league_id = 0u;
            if (active_index >= 0) {
                KboMilitaryActiveLoan* active = kbo_active_military_loan_at(active_index);
                if (active != NULL) {
                    original_team_id = active->original_team_id;
                    original_league_id = active->original_league_id;
                }
            }
            if (original_team_id == 0u) {
                original_team_id = active_team_id != 0u
                    ? active_team_id
                    : *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
            }

            if (original_team_id == 0u || team_id != original_team_id) {
                uint32_t today = kbo_military_policy_current_yyyymmdd();
                kbo_record_recent_military_fa_block(player_id, team_id, today);

                static volatile LONG service_block_log_count = 0;
                LONG slot = InterlockedIncrement(&service_block_log_count);
                if (slot <= 300) {
                    append_logf(
                        "KBO military team-add blocked active service transfer player=%u target_team=%u target_league=%u original_team=%u original_league=%u current_team=%u current_league=%u active_team=%u loan_team=%u days_left=%d military_active=%u active_index=%d today=%u",
                        player_id,
                        team_id,
                        team_league_id,
                        original_team_id,
                        original_league_id,
                        current_team_id,
                        current_league_id,
                        active_team_id,
                        loan_team_id,
                        days_left,
                        (unsigned)military_active,
                        active_index,
                        today);
                }
                KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.blocked_active_service_transfer");
                return 1;
            }
        }

        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.non_military_team");
        return 0;
    }

    if (active_index >= 0
            || (military_active != 0u && days_left > 0)
            || (loan_team_id == team_id && days_left > 0)) {
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.allowed_active_loan");
        return 0;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, team_id, today);

    static volatile LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 300) {
        append_logf(
            "KBO military team-add blocked player=%u team=%u league=%u current_team=%u current_league=%u active_team=%u loan_team=%u days_left=%d military_active=%u active_index=%d today=%u",
            player_id,
            team_id,
            team_league_id,
            current_team_id,
            current_league_id,
            active_team_id,
            loan_team_id,
            days_left,
            (unsigned)military_active,
            active_index,
            today);
    }
    KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.blocked");
    return 1;
}
