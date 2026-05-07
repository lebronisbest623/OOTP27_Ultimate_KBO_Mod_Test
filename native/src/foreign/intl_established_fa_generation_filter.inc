/* Generation-time filter for international established free agents. */

static int kbo_intl_established_fa_generation_filter_enabled(void)
{
    return !read_kbo_localappdata_flag_file("disable_intl_established_fa_generation_filter.txt");
}

__declspec(noinline) uint8_t ootp_kbo_intl_established_fa_generation_filter_allows_wrapper(
    uintptr_t player_ptr,
    uintptr_t league_ptr)
{
    if (!kbo_intl_established_fa_generation_filter_enabled()) {
        return 1u;
    }
    if (player_ptr == 0 || league_ptr == 0) {
        return 1u;
    }

    uint32_t primary_id = 0u;
    uint32_t fallback_id = 0u;
    if (!kbo_intl_established_fa_league_matches(league_ptr, &primary_id, &fallback_id)) {
        return 1u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 1u;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 1u;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    if (position_group != 1u
            || kbo_nation_is_asian_quota_candidate(nation_id)
            || !kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
        return 1u;
    }

    LONG slot = InterlockedIncrement(&g_kbo_intl_established_fa_generation_filter_block_count);
    if (slot <= 160) {
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        int32_t value_score = kbo_foreign_waiver_value_score(player);
        append_logf(
            "international established FA generation filter rejected #%ld player=%u nation=%u asian_quota=0 age=%d pos=%u/%u league_id=%u/%u score=%d action=retry_without_registering",
            slot,
            player_id,
            nation_id,
            (int)age,
            position_group,
            position_role,
            primary_id,
            fallback_id,
            value_score);
    } else if (slot == 161) {
        append_log_line("international established FA generation filter rejected log suppressed after 160 players");
    }

    return 0u;
}
