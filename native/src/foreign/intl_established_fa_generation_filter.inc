/* Generation-time filter for international established free agents. */

#define KBO_INTL_ESTABLISHED_FA_CATCHER_ALLOW_ONE_IN 250u

static int kbo_intl_established_fa_generation_filter_enabled(void)
{
    return kbo_get_foreign_fa_quality_cap_enabled_setting();
}

static int kbo_intl_established_fa_player_is_catcher(uint8_t position_group, uint8_t position_role)
{
    return position_group == 2u || (position_group != 1u && position_role == 2u);
}

static uint32_t kbo_intl_established_fa_mix_u32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static int kbo_intl_established_fa_catcher_rarity_allows(uint32_t player_id, uint32_t nation_id)
{
    uint32_t mixed = kbo_intl_established_fa_mix_u32(
        player_id ^ (nation_id * 0x9e3779b9u) ^ 0xC0A7C4E5u);
    return (mixed % KBO_INTL_ESTABLISHED_FA_CATCHER_ALLOW_ONE_IN) == 0u;
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

    int asian_quota = kbo_nation_is_asian_quota_candidate(nation_id);
    int32_t value_score = kbo_foreign_waiver_value_score(player);
    int32_t quality_cap = kbo_intl_established_fa_quality_score_cap(
        asian_quota,
        position_group,
        position_role);
    const char* reject_reason = NULL;
    if (kbo_intl_established_fa_player_is_catcher(position_group, position_role)
            && !kbo_intl_established_fa_catcher_rarity_allows(player_id, nation_id)) {
        reject_reason = "catcher_rarity";
    } else if (position_group == 1u
            && !asian_quota
            && kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
        reject_reason = "non_asian_bullpen";
    } else if (quality_cap > 0 && value_score > quality_cap) {
        reject_reason = kbo_intl_established_fa_quality_policy_label(
            asian_quota,
            position_group,
            position_role);
    }
    if (reject_reason == NULL) {
        return 1u;
    }

    LONG slot = InterlockedIncrement(&g_kbo_intl_established_fa_generation_filter_block_count);
    if (slot <= 160) {
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        append_logf(
            "international established FA generation filter rejected #%ld reason=%s player=%u nation=%u asian_quota=%d age=%d pos=%u/%u league_id=%u/%u score=%d cap=%d action=retry_without_registering",
            slot,
            reject_reason,
            player_id,
            nation_id,
            asian_quota,
            (int)age,
            position_group,
            position_role,
            primary_id,
            fallback_id,
            value_score,
            quality_cap);
    } else if (slot == 161) {
        append_log_line("international established FA generation filter rejected log suppressed after 160 players");
    }

    return 0u;
}
