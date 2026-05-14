#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "military_fa_player_action_context.h"
#include "military_fa_policy.h"
#include "../events/policy/military_selection_policy.h"
#include "../../players/team_policy/military_service_team_policy.h"

static volatile LONG g_kbo_military_fa_block_player_id = 0;
static volatile LONG g_kbo_military_fa_block_requester_team_id = 0;
static volatile LONG g_kbo_military_fa_block_date = 0;
static volatile LONG64 g_kbo_military_fa_block_tick = 0;

uint32_t kbo_military_policy_current_yyyymmdd(void)
{
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        kbo_get_current_yyyymmdd(&today);
    }
    return today;
}

void kbo_record_recent_military_fa_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    if (player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    InterlockedExchange(&g_kbo_military_fa_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_military_fa_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_military_fa_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_military_fa_block_tick, (LONG64)GetTickCount64());
}

static int kbo_recent_military_fa_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_military_fa_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_military_fa_block_date, 0, 0);
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_military_fa_block_tick, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (cached_tick <= 0
            || now < (ULONGLONG)cached_tick
            || now - (ULONGLONG)cached_tick > (ULONGLONG)policy->fa_recent_block_ttl_ms) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_military_fa_block_requester_team_id, 0, 0);
    if (requester <= 0 || !kbo_team_id_is_military_service_team((uint32_t)requester)) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    return 1;
}

int kbo_military_fa_candidate_fast_block(
    uintptr_t player_ptr,
    uint32_t requester_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || !kbo_team_id_is_military_service_team(requester_team_id)) {
        return 0;
    }
    if (context != NULL && strcmp(context, "signability") == 0) {
        return 0;
    }

    uint32_t player_id = 0u;
    if (player_ptr != 0
            && memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    }
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, requester_team_id, today);

    static volatile LONG military_fast_block_log_count = 0;
    LONG slot = InterlockedIncrement(&military_fast_block_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (slot <= policy->fa_fast_block_log_max) {
        append_logf(
            "FA fast-block source=%s reason=military_service_team player=%u requester_team=%u",
            context != NULL ? context : "",
            player_id,
            requester_team_id);
    }
    return 1;
}

int kbo_military_offer_eligibility_should_block(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uint8_t original_result,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (team_id <= 0 || !kbo_team_id_is_military_service_team((uint32_t)team_id)) {
        return 0;
    }
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, (uint32_t)team_id, today);

    static volatile LONG military_team_offer_log_count = 0;
    LONG slot = InterlockedIncrement(&military_team_offer_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (slot <= policy->fa_offer_eligibility_log_max) {
        append_logf(
            "military service team FA offer eligibility blocked player=%u requester_team=%d original=%u flag=%d",
            player_id,
            team_id,
            (uint32_t)original_result,
            flag);
    }
    return 1;
}

int kbo_military_signability_should_block(
    uint32_t player_id,
    int32_t requesting_team_id,
    int original_signability,
    uintptr_t caller_rva)
{
    if (player_id == 0u
            || requesting_team_id <= 0
            || !kbo_team_id_is_military_service_team((uint32_t)requesting_team_id)) {
        return 0;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, (uint32_t)requesting_team_id, today);

    static volatile LONG military_team_signability_log_count = 0;
    LONG slot = InterlockedIncrement(&military_team_signability_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (slot <= policy->fa_signability_log_max) {
        append_logf(
            "military service team FA signability blocked player=%u requester_team=%d original=%d caller_rva=0x%llx",
            player_id,
            requesting_team_id,
            original_signability,
            (unsigned long long)caller_rva);
    }
    return 1;
}

int kbo_military_submit_offer_should_block(uintptr_t screen_ptr, uint32_t player_id, uint32_t today)
{
    uint32_t military_team_id = 0u;
    if (!kbo_recent_military_fa_block_matches(player_id, today, &military_team_id)) {
        return 0;
    }

    static LONG military_submit_block_log_count = 0;
    LONG block_slot = InterlockedIncrement(&military_submit_block_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (block_slot <= policy->fa_submit_block_log_max) {
        append_logf(
            "military service team submit-offer blocked: screen=%p player=%u requester_team=%u today=%u",
            (void*)screen_ptr,
            player_id,
            military_team_id,
            today);
    }
    return 1;
}

static int kbo_military_player_assignment_matches_team(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    return current_team_id == team_id
        || active_team_id == team_id
        || loan_team_id == team_id;
}

int kbo_military_submit_offer_screen_should_block(
    uintptr_t screen_ptr,
    uint32_t player_id,
    uint32_t today,
    const char* source)
{
    if (screen_ptr == 0 || player_id == 0u || today == 0u) {
        return 0;
    }

    uint32_t team_offset = 0xffffffffu;
    uintptr_t team_ptr = 0;
    uintptr_t owner = 0;
    uint32_t team_id = 0u;
    const char* team_source = "screen";

    if (memory_range_readable(
            (void*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_OWNER_OFFSET),
            sizeof(uintptr_t))) {
        owner = *(uintptr_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_OWNER_OFFSET);
        team_id = kbo_military_fa_context_find_team_id(owner, &team_offset, &team_ptr);
        if (team_id != 0u) {
            team_source = "owner";
        }
    }

    if (team_id == 0u) {
        team_id = kbo_military_fa_context_find_team_id(screen_ptr, &team_offset, &team_ptr);
        if (team_id != 0u && kbo_military_player_assignment_matches_team(player_id, team_id)) {
            team_id = 0u;
            team_ptr = 0;
            team_offset = 0xffffffffu;
        }
    }

    if (team_id == 0u) {
        return 0;
    }

    kbo_record_recent_military_fa_block(player_id, team_id, today);

    static LONG military_submit_direct_block_log_count = 0;
    LONG block_slot = InterlockedIncrement(&military_submit_direct_block_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (block_slot <= policy->fa_submit_block_log_max) {
        append_logf(
            "military service team submit-offer direct blocked: source=%s team_source=%s screen=%p owner=%p player=%u requester_team=%u team_off=0x%x team_ptr=%p today=%u",
            source != NULL ? source : "",
            team_source,
            (void*)screen_ptr,
            (void*)owner,
            player_id,
            team_id,
            team_offset,
            (void*)team_ptr,
            today);
    }
    return 1;
}

int kbo_military_ai_fa_candidate_should_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    int32_t insert_index)
{
    if (player_id == 0u
            || requester_team_id == 0u
            || !kbo_team_id_is_military_service_team(requester_team_id)) {
        return 0;
    }

    static LONG military_ai_block_log_count = 0;
    LONG slot = InterlockedIncrement(&military_ai_block_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (slot <= policy->fa_ai_block_log_max) {
        append_logf(
            "military service team AI FA status candidate blocked player=%u requester_team=%u index=%d",
            player_id,
            requester_team_id,
            insert_index);
    }
    return 1;
}

int kbo_military_player_action_should_block(
    uintptr_t action_context,
    int32_t action_id,
    uint8_t strict_check)
{
    uint32_t player_offset = 0xffffffffu;
    uint32_t player_id = 0u;
    uint8_t* player = kbo_military_player_action_context_find_player(action_context, &player_offset, &player_id);
    uint32_t team_offset = 0xffffffffu;
    uintptr_t team_ptr = 0;
    uint32_t military_team_id = kbo_military_player_action_context_find_team_id(action_context, &team_offset, &team_ptr);
    if (player == NULL) {
        return 0;
    }
    if (military_team_id == 0u) {
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        if (current_team_id == 0u
                && military_active == 0u
                && action_id >= kbo_military_selection_policy()->fa_action_id_min
                && action_id <= kbo_military_selection_policy()->fa_action_id_max) {
            static LONG military_action_miss_log_count = 0;
            LONG miss_slot = InterlockedIncrement(&military_action_miss_log_count);
            if (miss_slot <= 80) {
                append_logf(
                    "military service team player action probe missed team: context=%p action=0x%x strict=%u player=%u player_off=0x%x scan=0x%x",
                    (void*)action_context,
                    action_id,
                    (unsigned)strict_check,
                    player_id,
                    player_offset,
                    (unsigned)kbo_military_selection_policy()->fa_player_action_context_scan_bytes);
            }
        }
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    if (current_team_id != 0u
            || military_active != 0u
            || action_id < kbo_military_selection_policy()->fa_action_id_min
            || action_id > kbo_military_selection_policy()->fa_action_id_max) {
        return 0;
    }

    static LONG military_action_block_log_count = 0;
    LONG slot = InterlockedIncrement(&military_action_block_log_count);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    if (slot <= policy->fa_player_action_log_max) {
        append_logf(
            "military service team player action blocked: context=%p action=0x%x strict=%u player=%u team=%u player_off=0x%x team_off=0x%x team_ptr=%p",
            (void*)action_context,
            action_id,
            (unsigned)strict_check,
            player_id,
            military_team_id,
            player_offset,
            team_offset,
            (void*)team_ptr);
    }
    return 1;
}
