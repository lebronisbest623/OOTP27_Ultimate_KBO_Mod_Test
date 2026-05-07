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

    if (requesting_team_id > 0 && kbo_team_id_is_military_service_team((uint32_t)requesting_team_id)) {
        static volatile LONG military_team_signability_log_count = 0;
        LONG slot = InterlockedIncrement(&military_team_signability_log_count);
        if (slot <= 200) {
            append_logf(
                "military service team FA signability blocked player=%u requester_team=%d original=%d caller_rva=0x%llx",
                player_id,
                requesting_team_id,
                original_signability,
                (unsigned long long)caller_rva);
        }
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
