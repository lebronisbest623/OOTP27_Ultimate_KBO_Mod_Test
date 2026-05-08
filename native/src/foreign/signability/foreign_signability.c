#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../bootstrap/hook_entrypoints.h"
#include "../../core/core_flags/flags_api.h"
#include "../../core/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/team_lookup.h"
#include "../foreign_waiver_date.h"
#include "../foreign_waiver_player_eval.h"
#include "../foreign_waiver_policy.h"
#include "../injury/foreign_injury.h"
#include "../rights/foreign_waiver_rights_query.h"
#include "../../military_service/military_service.h"
#include "foreign_fa_block_state.h"

/* ---- native/src/foreign/signability/foreign_fa_block_log.inc ---- */
/* Foreign FA/signability diagnostic logging. */

static void kbo_log_foreign_signability_block_callsite(
    uintptr_t caller_rva,
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today)
{
    enum { KBO_SIGNABILITY_CALLSITE_LOG_SLOTS = 64 };
    static volatile LONG logged_count = 0;
    static uintptr_t logged_callers[KBO_SIGNABILITY_CALLSITE_LOG_SLOTS] = {0};

    if (caller_rva == 0) {
        return;
    }
    for (int i = 0; i < KBO_SIGNABILITY_CALLSITE_LOG_SLOTS; i++) {
        if (logged_callers[i] == caller_rva) {
            return;
        }
    }

    LONG slot = InterlockedIncrement(&logged_count) - 1;
    if (slot < 0 || slot >= KBO_SIGNABILITY_CALLSITE_LOG_SLOTS) {
        return;
    }
    logged_callers[slot] = caller_rva;
    append_logf(
        "foreign reserve signability: new block callsite caller_rva=0x%llx player=%u requester_team=%u holder_team=%u original=%d today=%u",
        (unsigned long long)caller_rva,
        player_id,
        requester_team_id,
        holder_team_id,
        original_signability,
        today);
}

/* ---- native/src/foreign/signability/foreign_fa_fast_block_policy.inc ---- */
/* Early FA candidate block policy. */

static int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || requesting_team_id <= 0) {
        return 0;
    }

    uint32_t requester_team_id = (uint32_t)requesting_team_id;
    if (kbo_military_fa_candidate_fast_block(player_ptr, requester_team_id, context, out_player_id)) {
        return 1;
    }

    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }
    if (player_id == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return 0;
    }

    if (kbo_foreign_waiver_ai_enabled()) {
        kbo_prune_expired_foreign_waiver_rights(today);

        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u
                && holder_team_id != requester_team_id) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
            kbo_record_recent_foreign_offer_block(player_id, requester_team_id, holder_team_id, today);

            static volatile LONG rights_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&rights_fast_block_log_count);
            if (slot <= 40) {
                append_logf(
                    "FA fast-block source=%s reason=foreign_reserve player=%u requester_team=%u holder_team=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    holder_team_id,
                    today);
            } else if (slot == 41) {
                append_log_line("FA fast-block foreign_reserve log suppressed after 40 entries");
            }
            return 1;
        }
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            static volatile LONG custom_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_fast_block_log_count);
            if (slot <= 40) {
                append_logf(
                    "FA fast-block source=%s reason=custom_foreign_policy player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            } else if (slot == 41) {
                append_log_line("FA fast-block custom_foreign_policy log suppressed after 40 entries");
            }
            return 1;
        }
    }

    return 0;
}

/* ---- native/src/foreign/signability/foreign_signability_block_policy.inc ---- */
/* Foreign-player signability block and adjustment policy. */

static int kbo_no_minor_contract_signability_floor(int signability)
{
    if (!kbo_fix_enabled()) {
        return signability;
    }
    if (signability > 0 && signability < 4) {
        return 4;
    }
    return signability;
}

static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva)
{
    (void)year_hint;
    if (!kbo_fix_enabled()) {
        return original_signability;
    }
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return original_signability;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        return original_signability;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return original_signability;
    }

    if (kbo_military_signability_should_block(
            player_id,
            requesting_team_id,
            original_signability,
            caller_rva)) {
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    kbo_log_asian_quota_signability_probe(player, player_id, requesting_team_id, original_signability, caller_rva);

    uint32_t today = 0;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_signability;
    }

    uint32_t holder_team_id = 0;
    if (kbo_foreign_waiver_ai_enabled()) {
        kbo_prune_expired_foreign_waiver_rights(today);

        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

            if (requesting_team_id <= 0) {
                static LONG generic_block_log_count = 0;
                LONG slot = InterlockedIncrement(&generic_block_log_count);
                if (slot <= 200) {
                    append_logf(
                        "foreign reserve signability: blocked generic request player=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                        player_id,
                        holder_team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                return 0; /* OOTP signability enum: 0 = Impossible. */
            }

            uint32_t team_id = (uint32_t)requesting_team_id;
            if (caller_rva == 0x1b0e3e9u) {
                static LONG display_block_log_count = 0;
                LONG display_slot = InterlockedIncrement(&display_block_log_count);
                if (display_slot <= 120) {
                    append_logf(
                        "foreign reserve signability: blocked display request player=%u requester_team=%u holder_team=%u original=%d today=%u caller_rva=0x%llx",
                        player_id,
                        team_id,
                        holder_team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
                return 0; /* This callsite is a FA-list display probe, not a reliable holder-team offer check. */
            }

            if (team_id == holder_team_id) {
                kbo_record_recent_foreign_offer_allow(player_id, team_id, today);
                static LONG allow_log_count = 0;
                LONG slot = InterlockedIncrement(&allow_log_count);
                if (slot <= 40) {
                    append_logf(
                        "foreign reserve signability: holder allowed player=%u team=%u original=%d today=%u caller_rva=0x%llx",
                        player_id,
                        team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                return original_signability;
            }

            static LONG block_log_count = 0;
            LONG slot = InterlockedIncrement(&block_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign reserve signability: blocked player=%u requester_team=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                    player_id,
                    team_id,
                    holder_team_id,
                    original_signability,
                    today,
                    (unsigned long long)caller_rva);
            }
            kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
            kbo_log_foreign_signability_block_callsite(caller_rva, player_id, team_id, holder_team_id, original_signability, today);
            return 0; /* OOTP signability enum: 0 = Impossible. */
        }
    }

    if (requesting_team_id > 0
            && kbo_custom_foreign_policy_enabled()
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)requesting_team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(
            player,
            (uint32_t)requesting_team_id);
        int adjusted = allowed ? original_signability : 0;
        if (allowed && original_signability == 0 && override_original_block) {
            adjusted = 4;
        }
        adjusted = kbo_no_minor_contract_signability_floor(adjusted);

        static volatile LONG custom_policy_signability_log_count = 0;
        LONG slot = InterlockedIncrement(&custom_policy_signability_log_count);
        if (slot <= 200) {
            append_logf(
                "custom foreign policy signability player=%u requester_team=%d original=%d adjusted=%d allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u caller_rva=0x%llx",
                player_id,
                requesting_team_id,
                original_signability,
                adjusted,
                allowed,
                override_original_block,
                effective_before,
                effective_after,
                effective_limit,
                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                injured_player_id,
                today,
                (unsigned long long)caller_rva);
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)requesting_team_id, today);
        } else if (adjusted != 0) {
            kbo_record_recent_custom_foreign_policy_allow(player_id, (uint32_t)requesting_team_id, today);
        }
        return adjusted;
    }

    if (requesting_team_id > 0) {
        uint8_t injury_slot_type = 0u;
        uint32_t injured_player_id = 0u;
        uint32_t effective_count = 0u;
        uint32_t effective_limit = 0u;
        if (kbo_foreign_injury_replacement_signing_exception_available(
                (uint32_t)requesting_team_id,
                player,
                &injury_slot_type,
                &injured_player_id,
                &effective_count,
                &effective_limit)) {
            static LONG injury_signability_log_count = 0;
            LONG slot = InterlockedIncrement(&injury_signability_log_count);
            int adjusted = original_signability != 0 ? original_signability : 4;
            adjusted = kbo_no_minor_contract_signability_floor(adjusted);
            if (slot <= 120) {
                append_logf(
                    "foreign injury replacement signability allowed player=%u requester_team=%d injured=%u slot=%s original=%d adjusted=%d effective=%u limit=%u today=%u caller_rva=0x%llx",
                    player_id,
                    requesting_team_id,
                    injured_player_id,
                    kbo_foreign_injury_slot_label(injury_slot_type),
                    original_signability,
                    adjusted,
                    effective_count,
                    effective_limit,
                    today,
                    (unsigned long long)caller_rva);
            }
            return adjusted;
        }
    }

    return kbo_no_minor_contract_signability_floor(original_signability);
}

/* ---- native/src/foreign/signability/foreign_signability_wrapper.inc ---- */
/* Player/team signability hook wrapper. Included from native/KBOFix.c. */

typedef int (__fastcall *OotpPlayerTeamSignabilityFn)(void* player, int32_t team_id, uint16_t year_hint);

__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uint16_t year_hint,
    uintptr_t original_func_ptr)
{
    OotpPlayerTeamSignabilityFn original_func = (OotpPlayerTeamSignabilityFn)original_func_ptr;

    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "signability", NULL)) {
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    int original_signability = 0;
    if (original_func != NULL) {
        original_signability = original_func((void*)player_ptr, team_id, year_hint);
    }
    uintptr_t caller_rva = 0;
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    if (exe != NULL && caller > (uintptr_t)exe) {
        caller_rva = caller - (uintptr_t)exe;
    }
    return kbo_enforce_foreign_waiver_signability(player_ptr, team_id, year_hint, original_signability, caller_rva);
}

/* ---- native/src/foreign/signability/foreign_offer_eligibility_wrapper.inc ---- */
/* Player offer-eligibility hook wrapper. Included from native/KBOFix.c. */

__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uintptr_t original_func_ptr)
{
    typedef uint8_t (__fastcall *OriginalOfferEligibilityFn)(void*, int32_t, int32_t);
    OriginalOfferEligibilityFn original_func = (OriginalOfferEligibilityFn)original_func_ptr;

    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "offer_eligibility", NULL)) {
        return 0;
    }

    uint8_t original_result = 0;
    if (original_func != NULL) {
        original_result = original_func((void*)player_ptr, team_id, flag);
    }

    if (player_ptr == 0 || team_id <= 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return original_result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    kbo_log_asian_quota_offer_probe(player, player_id, team_id, original_result, flag);

    if (kbo_military_offer_eligibility_should_block(
            player_ptr,
            team_id,
            flag,
            original_result,
            &player_id)) {
        return 0;
    }

    uint32_t today = 0u;
    uint32_t holder_team_id = 0u;
    if (player_id == 0u || !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_result;
    }

    if (kbo_foreign_waiver_ai_enabled()
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            && holder_team_id != 0u) {
        if (holder_team_id == (uint32_t)team_id) {
            kbo_record_recent_foreign_offer_allow(player_id, (uint32_t)team_id, today);
            return original_result;
        }

        kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 80) {
            append_logf(
                "foreign reserve offer eligibility blocked player=%u requester_team=%d holder_team=%u original=%u flag=%d today=%u",
                player_id,
                team_id,
                holder_team_id,
                (uint32_t)original_result,
                flag,
                today);
        }

        kbo_record_recent_foreign_offer_block(player_id, (uint32_t)team_id, holder_team_id, today);
        return 0;
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(player, (uint32_t)team_id);
        uint8_t adjusted = allowed ? original_result : 0u;
        if (allowed && original_result == 0u && override_original_block) {
            adjusted = 4u;
        }
        if (adjusted > 0u && adjusted < 4u) {
            adjusted = 4u;
        }

        static LONG custom_policy_offer_log_enabled_initialized = 0;
        static LONG custom_policy_offer_log_enabled = 0;
        if (InterlockedCompareExchange(&custom_policy_offer_log_enabled_initialized, 1, 0) == 0) {
            InterlockedExchange(
                &custom_policy_offer_log_enabled,
                read_kbo_localappdata_flag_file("enable_kbo_custom_foreign_offer_logs.txt") ? 1 : 0);
        }
        static volatile LONG custom_policy_offer_log_count = 0;
        LONG offer_log_slot = InterlockedIncrement(&custom_policy_offer_log_count);
        if (offer_log_slot <= 120 || InterlockedCompareExchange(&custom_policy_offer_log_enabled, 0, 0) != 0) {
            if (offer_log_slot <= 240) {
                append_logf(
                    "custom foreign policy offer player=%u requester_team=%d original=%u adjusted=%u allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u flag=%d today=%u",
                    player_id,
                    team_id,
                    (uint32_t)original_result,
                    (uint32_t)adjusted,
                    allowed,
                    override_original_block,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    flag,
                    today);
            }
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)team_id, today);
        } else if (adjusted != 0u && flag != 0) {
            kbo_record_custom_foreign_pending_offer((uint32_t)team_id, player, today);
            kbo_record_recent_custom_foreign_policy_allow(player_id, (uint32_t)team_id, today);
        } else if (adjusted != 0u) {
            kbo_record_recent_custom_foreign_policy_allow(player_id, (uint32_t)team_id, today);
        }
        return adjusted;
    }

    uint8_t injury_slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_limit = 0u;
    if (kbo_foreign_injury_replacement_signing_exception_available(
            (uint32_t)team_id,
            player,
            &injury_slot_type,
            &injured_player_id,
            &effective_count,
            &effective_limit)) {
        static volatile LONG injury_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&injury_offer_log_count);
        uint8_t adjusted = original_result != 0u ? original_result : 4u;
        if (adjusted > 0u && adjusted < 4u) {
            adjusted = 4u;
        }
        if (slot <= 120) {
            append_logf(
                "foreign injury replacement offer eligibility allowed player=%u requester_team=%d injured=%u slot=%s original=%u adjusted=%u effective=%u limit=%u flag=%d today=%u",
                player_id,
                team_id,
                injured_player_id,
                kbo_foreign_injury_slot_label(injury_slot_type),
                (uint32_t)original_result,
                (uint32_t)adjusted,
                effective_count,
                effective_limit,
                flag,
                today);
        }
        return adjusted;
    }

    return original_result;
}

/* ---- native/src/foreign/signability/foreign_ai_fa_candidate_wrapper.inc ---- */
/* AI FA status candidate hook wrapper. Included from native/KBOFix.c. */

__declspec(noinline) int32_t ootp_kbo_ai_fa_status_candidate_insert_wrapper(
    uintptr_t frame_ptr,
    uintptr_t player_ptr,
    int32_t insert_index,
    uintptr_t candidate_array)
{
    if (frame_ptr == 0 || player_ptr == 0 || insert_index < 0) {
        return insert_index;
    }
    if (!memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return insert_index;
    }

    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0;
    uint32_t holder_team_id = 0;
    uint32_t requester_team_id = 0;
    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + OOTP27_AI_FA_STATUS_FRAME_TEAM_PTR_OFFSET);
    if (team_ptr != 0 && memory_range_readable((void*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET), sizeof(uint32_t))) {
        requester_team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    }

    if (requester_team_id != 0u
            && kbo_fast_block_fa_candidate_before_original(
                player_ptr,
                (int32_t)requester_team_id,
                "ai_fa_status_candidate",
                &player_id)) {
        return insert_index;
    }

    if (kbo_military_ai_fa_candidate_should_block(player_id, requester_team_id, insert_index)) {
        return insert_index;
    }

    if (player_id != 0u
        && requester_team_id != 0u
        && kbo_get_foreign_waiver_current_yyyymmdd(&today)
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u
        && holder_team_id != requester_team_id) {
        static LONG block_log_count = 0;
        LONG slot = InterlockedIncrement(&block_log_count);
        if (slot <= 300) {
            append_logf(
                "foreign reserve AI FA status candidate blocked player=%u requester_team=%u holder_team=%u index=%d today=%u",
                player_id,
                requester_team_id,
                holder_team_id,
                insert_index,
                today);
        }
        return insert_index;
    }

    if (player_id != 0u
            && requester_team_id != 0u
            && kbo_custom_foreign_policy_enabled()
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)player_ptr)) {
        if (today == 0u && !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            today = 0u;
        }
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            (uint8_t*)player_ptr,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            static LONG custom_policy_ai_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_policy_ai_block_log_count);
            if (slot <= 300) {
                append_logf(
                    "custom foreign policy AI FA status candidate blocked player=%u requester_team=%u index=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    requester_team_id,
                    insert_index,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            return insert_index;
        }
    }

    if (candidate_array == 0) {
        return insert_index;
    }
    if (!memory_range_readable((void*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))), sizeof(uintptr_t))) {
        return insert_index;
    }
    *(uintptr_t*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))) = player_ptr;
    *(int32_t*)(frame_ptr - OOTP27_AI_FA_STATUS_FRAME_INSERT_COUNT_DELTA) = insert_index + 1;
    return insert_index + 1;
}

/* ---- native/src/foreign/signability/foreign_trade_check_wrapper.inc ---- */
/* League trade validation hook wrapper. Included from native/KBOFix.c. */

#define KBO_TRADE_CHECK_RESULT_FOREIGN_QUOTA_BLOCK (-9)

__declspec(noinline) int ootp_kbo_trade_check_foreign_policy_probe(
    uintptr_t trade_ptr,
    int32_t side)
{
    if (kbo_fix_enabled() && kbo_custom_foreign_policy_enabled()) {
        int blocked_side = -1;
        uint32_t team_id = 0u;
        uint32_t incoming_player_id = 0u;
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        int allowed = kbo_custom_foreign_policy_trade_allows(
            trade_ptr,
            side,
            &blocked_side,
            &team_id,
            &incoming_player_id,
            &effective_before,
            &effective_after,
            &effective_limit);
        if (!allowed) {
            static volatile LONG trade_block_log_count = 0;
            LONG slot = InterlockedIncrement(&trade_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "custom foreign policy trade blocked trade=%p request_side=%d blocked_side=%d team=%u incoming_player=%u effective_before=%u effective_after=%u limit=%u",
                    (void*)trade_ptr,
                    side,
                    blocked_side,
                    team_id,
                    incoming_player_id,
                    effective_before,
                    effective_after,
                    effective_limit);
            }
            return 0;
        }
    }
    return 1;
}

