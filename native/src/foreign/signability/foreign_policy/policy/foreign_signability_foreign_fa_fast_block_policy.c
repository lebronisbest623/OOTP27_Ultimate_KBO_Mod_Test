#include "../internal/foreign_signability_internal.h"

/* Early FA candidate block policy. */

int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    KBO_PROFILE_BEGIN(profile_fa_fast_block);
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || requesting_team_id <= 0) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.disabled");
        return 0;
    }

    uint32_t requester_team_id = (uint32_t)requesting_team_id;
    if (kbo_military_fa_candidate_fast_block(player_ptr, requester_team_id, context, out_player_id)) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.military");
        return 1;
    }

    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.bad_player");
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }
    if (player_id == 0u) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.bad_player_id");
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.no_date");
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
            KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.reserve_blocked");
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
            KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.custom_blocked");
            return 1;
        }
    }

    KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.allowed");
    return 0;
}

