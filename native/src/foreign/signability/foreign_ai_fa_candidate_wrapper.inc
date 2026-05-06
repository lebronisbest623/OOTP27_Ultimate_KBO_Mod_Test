/* AI FA status candidate hook wrapper. Included from native/src/foreign/foreign_signability_hooks.inc. */

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
    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + 0x140u);
    if (team_ptr != 0 && memory_range_readable((void*)(team_ptr + 0x4450u), sizeof(uint32_t))) {
        requester_team_id = *(uint32_t*)(team_ptr + 0x4450u);
    }

    if (requester_team_id != 0u
            && kbo_fast_block_fa_candidate_before_original(
                player_ptr,
                (int32_t)requester_team_id,
                "ai_fa_status_candidate",
                &player_id)) {
        return insert_index;
    }

    if (player_id != 0u
        && requester_team_id != 0u
        && kbo_team_id_is_military_service_team(requester_team_id)) {
        static LONG military_ai_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_ai_block_log_count);
        if (slot <= 300) {
            append_logf(
                "military service team AI FA status candidate blocked player=%u requester_team=%u index=%d",
                player_id,
                requester_team_id,
                insert_index);
        }
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
    *(int32_t*)(frame_ptr - 0x2c) = insert_index + 1;
    return insert_index + 1;
}
