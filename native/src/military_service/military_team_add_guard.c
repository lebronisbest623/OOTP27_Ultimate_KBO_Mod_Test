#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../amateur_player_quality/amateur_player_quality.h"
#include "../bootstrap/hook_entrypoints.h"
#include "../bootstrap/ootp_offsets.h"
#include "../bootstrap/profiler.h"
#include "../core/core_flags/flags_api.h"
#include "../core/core_log.h"
#include "../fa_compensation/fa_compensation_history.h"
#include "../fa_filing/fa_filing.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_lookup.h"
#include "military_active_loan.h"
#include "military_fa_policy.h"
#include "military_player_state.h"
#include "military_service_team_policy.h"
#include "military_team_add_guard.h"

typedef uint8_t (__fastcall *OotpKboTeamAddPlayerExFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);

static OotpKboTeamAddPlayerExFn g_kbo_team_add_player_guard_trampoline = NULL;

void kbo_set_team_add_player_guard_trampoline(void* trampoline)
{
    g_kbo_team_add_player_guard_trampoline = (OotpKboTeamAddPlayerExFn)(uintptr_t)trampoline;
}

void kbo_clear_team_add_player_guard_trampoline(void)
{
    g_kbo_team_add_player_guard_trampoline = NULL;
}

static void kbo_team_add_player_record_fa_compensation_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    KBO_PROFILE_BEGIN(profile_fa_comp_probe_inner);
    if (!kbo_fix_enabled()
            || read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt")
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.precheck_reject");
        return;
    }

    if (before_current_team_id != 0u && before_active_team_id != 0u) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.not_teamless_before");
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id == 0u || league_id == 0u || kbo_team_id_is_military_service_team(team_id)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.bad_team");
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t filing_original_team_id = 0u;
    uint32_t filing_league_id = 0u;
    uint32_t filing_season = 0u;
    if (!kbo_fa_filing_find_latest_player(
            player_id,
            &filing_original_team_id,
            &filing_league_id,
            &filing_season)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.no_filing");
        return;
    }

    if (filing_league_id != 0u) {
        league_id = filing_league_id;
    }

    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t signing_team_id = after_active_team_id != 0u ? after_active_team_id : team_id;
    if (signing_team_id == filing_original_team_id) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.same_team");
        return;
    }
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        append_logf(
            "KBO team-add FA compensation probe player=%u team=%u league=%u before_current=%u before_active=%u before_original=%u filing_original=%u filing_league=%u filing_season=%u after_current=%u after_active=%u",
            player_id,
            signing_team_id,
            league_id,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id,
            filing_original_team_id,
            filing_league_id,
            filing_season,
            after_current_team_id,
            after_active_team_id);
    }

    kbo_record_fa_compensation_signing(player_ptr, signing_team_id, league_id, "team_add_player_success");
    KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.record_attempt");
}

static int kbo_military_team_add_player_should_block(uintptr_t team_ptr, uintptr_t player_ptr)
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
    if (!kbo_team_id_is_military_service_team(team_id)) {
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.non_military_team");
        return 0;
    }

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

    if (active_index >= 0
            || (military_active != 0u && days_left > 0)
            || (loan_team_id == team_id && days_left > 0)) {
        KBO_PROFILE_END(profile_military_team_add_should_block, "military.team_add_should_block.allowed_active_loan");
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
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

__declspec(noinline) uint8_t ootp_kbo_team_add_player_guard_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    KBO_PROFILE_BEGIN(profile_team_add_guard_wrapper);
    if (kbo_military_team_add_player_should_block(team_ptr, player_ptr)) {
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.blocked");
        return 0;
    }

    OotpKboTeamAddPlayerExFn original = g_kbo_team_add_player_guard_trampoline;
    if (original == NULL) {
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.no_original");
        return 0;
    }

    uint32_t before_current_team_id = 0u;
    uint32_t before_active_team_id = 0u;
    uint32_t before_original_team_id = 0u;
    if (kbo_player_pointer_plausible(player_ptr)) {
        uint8_t* player = (uint8_t*)player_ptr;
        before_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        before_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            before_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        }
    }

    uintptr_t effective_team_ptr = kbo_amateur_team_add_player_reroute_before_original(
        team_ptr,
        player_ptr,
        "team_add_player_before_original");
    int amateur_pre_rerouted = effective_team_ptr != team_ptr;

    KBO_PROFILE_BEGIN(profile_team_add_original);
    uint8_t result = original(effective_team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
    KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original.success" : "team_add_guard.original.rejected");
    for (int amateur_retry = 0; result == 0u && amateur_pre_rerouted && amateur_retry < 4; amateur_retry++) {
        static volatile LONG fallback_log_count = 0;
        LONG fallback_slot = InterlockedIncrement(&fallback_log_count);
        uint32_t original_team_id = 0u;
        uint32_t effective_team_id = 0u;
        uint32_t player_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            original_team_id = *(uint32_t*)((uint8_t*)team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (memory_range_readable((void*)effective_team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            effective_team_id = *(uint32_t*)((uint8_t*)effective_team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (kbo_player_pointer_plausible(player_ptr)) {
            player_id = *(uint32_t*)((uint8_t*)player_ptr + OOTP27_PLAYER_ID_OFFSET);
        }
        if (effective_team_id != 0u) {
            uint8_t* rejected_team = (uint8_t*)effective_team_ptr;
            uint32_t rejected_league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(rejected_team);
            kbo_amateur_assignment_mark_rejected_target(rejected_league_id, effective_team_id);
        }
        if (fallback_slot <= 80) {
            append_logf(
                "amateur assignment reroute target rejected; retrying alternate player=%u original_team=%u rejected_team=%u attempt=%d",
                player_id,
                original_team_id,
                effective_team_id,
                amateur_retry + 1);
        }

        uintptr_t retry_team_ptr = kbo_amateur_team_add_player_reroute_before_original(
            team_ptr,
            player_ptr,
            "team_add_player_reroute_retry");
        if (retry_team_ptr == team_ptr || retry_team_ptr == effective_team_ptr) {
            break;
        }
        effective_team_ptr = retry_team_ptr;
        KBO_PROFILE_BEGIN(profile_team_add_original);
        result = original(effective_team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original_retry.success" : "team_add_guard.original_retry.rejected");
    }
    if (result == 0u && amateur_pre_rerouted) {
        KBO_PROFILE_BEGIN(profile_team_add_original);
        result = original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original_fallback.success" : "team_add_guard.original_fallback.rejected");
        if (result != 0u) {
            effective_team_ptr = team_ptr;
            amateur_pre_rerouted = 0;
        }
    }
    if (result != 0u) {
        KBO_PROFILE_BEGIN(profile_team_add_amateur_assignment);
        if (amateur_pre_rerouted) {
            kbo_amateur_team_add_player_note_original_success(
                effective_team_ptr,
                player_ptr,
                "team_add_player_pre_rerouted_original_success",
                result);
        } else {
            kbo_amateur_team_add_player_note_original_success(
                team_ptr,
                player_ptr,
                "team_add_player_original_success",
                result);
        }
        KBO_PROFILE_END(profile_team_add_amateur_assignment, "team_add_guard.amateur_assignment_after_original");

        KBO_PROFILE_BEGIN(profile_team_add_fa_comp);
        kbo_team_add_player_record_fa_compensation_success(
            effective_team_ptr,
            player_ptr,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_fa_comp, "team_add_guard.fa_comp_probe");
    }
    KBO_PROFILE_END(profile_team_add_guard_wrapper, result != 0u ? "team_add_guard.success" : "team_add_guard.original_rejected");
    return result;
}
