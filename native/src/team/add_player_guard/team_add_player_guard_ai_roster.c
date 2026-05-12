#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../lookup/team_lookup.h"
#include "ai_roster/internal/team_add_player_guard_ai_roster_internal.h"
#include "team_add_player_guard.h"
#include "team_add_player_guard_ai_roster.h"

static OotpKboAiRosterSelectFn g_kbo_ai_roster_select_trace_trampoline = NULL;
static OotpKboAiRosterContextFlowFn g_kbo_ai_roster_primary_apply_flow_trace_trampoline = NULL;
static OotpKboAiRosterApplySelectionFn g_kbo_ai_roster_apply_selection_trace_trampoline = NULL;

static int32_t kbo_read_ai_roster_select_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

void kbo_set_ai_roster_select_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_select_trace_trampoline = (OotpKboAiRosterSelectFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_primary_apply_flow_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_primary_apply_flow_trace_trampoline =
        (OotpKboAiRosterContextFlowFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_apply_selection_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_apply_selection_trace_trampoline =
        (OotpKboAiRosterApplySelectionFn)(uintptr_t)trampoline;
}

__declspec(noinline) uintptr_t ootp_kbo_ai_roster_select_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    OotpKboAiRosterSelectFn original = g_kbo_ai_roster_select_trace_trampoline;
    uintptr_t result_ptr = original != NULL
        ? original(context_ptr, slot_index, depth_hint)
        : 0u;

    uintptr_t slot_block_ptr = 0u;
    uintptr_t source_vector_ptr = 0u;
    int32_t source_count = -1;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        slot_block_ptr = kbo_ai_roster_context_slot_block(context_ptr, (uint16_t)slot_index);
        if (slot_block_ptr != 0u) {
            source_vector_ptr = slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET;
            source_count = kbo_pointer_vector_count(source_vector_ptr);
        }
    }

    int32_t rescue_source_index = -1;
    uint32_t rescue_active_team_id = 0u;
    int64_t rescue_score = 0;
    KboAiRosterForeignCandidateSummary rescue_summary;
    uintptr_t rescue_ptr = kbo_ai_roster_choose_source_select_rescue_candidate(
        source_vector_ptr,
        source_count,
        result_ptr,
        &rescue_source_index,
        &rescue_active_team_id,
        &rescue_score,
        &rescue_summary);
    if (rescue_ptr == 0u || rescue_ptr == result_ptr) {
        return result_ptr;
    }

    uint32_t native_player_id = 0u;
    int native_foreign = 0;
    int32_t native_score_fe0 = 0;
    int32_t native_score_fe4 = 0;
    int16_t native_overall = 0;
    int16_t native_ratings = 0;
    if (kbo_player_pointer_plausible(result_ptr)
            && memory_range_readable((void*)result_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t* native_player = (uint8_t*)result_ptr;
        native_player_id = *(uint32_t*)(native_player + OOTP27_PLAYER_ID_OFFSET);
        native_foreign = kbo_player_is_foreign_for_kbo_rights(native_player);
        native_score_fe0 = kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
        native_score_fe4 = kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
        native_overall = kbo_read_player_i16(native_player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
        native_ratings = kbo_read_player_i16(native_player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    }

    static volatile LONG rescue_log_count = 0;
    LONG rescue_slot = InterlockedIncrement(&rescue_log_count);
    append_logf(
        "ootp ai roster foreign source-select rescue #%ld caller_rva=0x%x context=%p slot_index=%d depth_hint=%d source_count=%d native=%u native_foreign=%d native_fe0=%d native_fe4=%d native_overall=%d native_ratings=%d override=%u source_idx=%d active_team=%u score=%lld current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f25=%u f62=%u f65=%u f06=%d fe0=%d fe4=%d overall=%d talent=%d ratings=%d",
        rescue_slot,
        caller_rva,
        (void*)context_ptr,
        slot_index,
        depth_hint,
        source_count,
        native_player_id,
        native_foreign,
        native_score_fe0,
        native_score_fe4,
        native_overall,
        native_ratings,
        rescue_summary.player_id,
        rescue_source_index,
        rescue_active_team_id,
        (long long)rescue_score,
        rescue_summary.current_team_id,
        rescue_summary.active_team_id,
        rescue_summary.league_id,
        rescue_summary.default_team_id,
        rescue_summary.status24,
        rescue_summary.status25,
        rescue_summary.status26,
        rescue_summary.f25,
        rescue_summary.f62,
        rescue_summary.f65,
        rescue_summary.f06,
        rescue_summary.score_fe0,
        rescue_summary.score_fe4,
        rescue_summary.overall,
        rescue_summary.talent,
        rescue_summary.ratings);
    return rescue_ptr;
}

__declspec(noinline) void ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper(
    uintptr_t context_ptr)
{
    KboAiRosterFlowContextSnapshot before;
    KboAiRosterFlowContextSnapshot after;
    kbo_ai_roster_flow_read_context(context_ptr, &before);

    OotpKboAiRosterContextFlowFn original = g_kbo_ai_roster_primary_apply_flow_trace_trampoline;
    if (original != NULL) {
        original(context_ptr);
    }

    kbo_ai_roster_flow_read_context(context_ptr, &after);
    if (kbo_ai_roster_context_flow_apply_rescue(
            context_ptr,
            &before,
            &after,
            g_kbo_ai_roster_apply_selection_trace_trampoline)) {
        kbo_ai_roster_flow_read_context(context_ptr, &after);
    }
}

__declspec(noinline) void ootp_kbo_ai_roster_apply_selection_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code)
{
    OotpKboAiRosterApplySelectionFn original = g_kbo_ai_roster_apply_selection_trace_trampoline;

    uintptr_t slot_block_ptr = 0u;
    uint32_t slot_team_id = 0u;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        slot_block_ptr = kbo_ai_roster_context_slot_block(context_ptr, (uint16_t)slot_index);
        slot_team_id = kbo_ai_roster_context_slot_team_id(context_ptr, (uint16_t)slot_index);
    }

    uint32_t before_slot_code = 0u;
    uint32_t before_slot_player_id = 0u;
    if (slot_block_ptr != 0u && target_slot >= 0 && target_slot <= KBO_AI_ROSTER_SLOT_MAX) {
        before_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, (uint16_t)target_slot);
        before_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, (uint16_t)target_slot);
    }

    int player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t player_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) : 0u;
    int incoming_foreign = player_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    DWORD rescue_age_ms = 0u;
    if (original != NULL
            && slot_block_ptr != 0u
            && target_slot >= 0
            && target_slot <= KBO_AI_ROSTER_SLOT_MAX
            && player_plausible
            && player_id != 0u
            && !incoming_foreign
            && before_slot_player_id != 0u
            && before_slot_player_id != player_id
            && kbo_ai_roster_recent_foreign_apply_rescue_match(
                context_ptr,
                slot_block_ptr,
                (uint16_t)target_slot,
                before_slot_player_id,
                &rescue_age_ms)) {
        uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
        HMODULE host_exe = GetModuleHandleA(NULL);
        uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
            ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
            : 0u;

        static volatile LONG shield_log_count = 0;
        LONG shield_slot = InterlockedIncrement(&shield_log_count);
        if (shield_slot <= 1000) {
            append_logf(
                "ootp ai roster foreign apply rescue shield #%ld caller_rva=0x%x context=%p slot_index=%d target_slot=%d roster_code=%d age_ms=%lu slot_block=%p slot_team=%u before_slot_code=%u before_slot_player=%u incoming_player=%u incoming_foreign=%d incoming_nation=%u incoming_current=%u incoming_active=%u incoming_league=%u incoming_status24=%u incoming_status25=%u incoming_status26=%u incoming_f62=%u incoming_f65=%u incoming_f68=%u incoming_f1a=%u incoming_score_fe0=%d incoming_score_fe4=%d incoming_overall=%d incoming_ratings=%d",
                shield_slot,
                caller_rva,
                (void*)context_ptr,
                slot_index,
                target_slot,
                roster_code,
                (unsigned long)rescue_age_ms,
                (void*)slot_block_ptr,
                slot_team_id,
                before_slot_code,
                before_slot_player_id,
                player_id,
                incoming_foreign,
                *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                0u,
                0u,
                0u,
                (uint32_t)player[0xf62u],
                (uint32_t)player[0xf65u],
                (uint32_t)player[0xf68u],
                (uint32_t)player[0xf1au],
                kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET),
                kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET),
                kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
                kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
        }
        return;
    }

    if (original != NULL) {
        original(context_ptr, slot_index, player_ptr, target_slot, roster_code);
    }
}
