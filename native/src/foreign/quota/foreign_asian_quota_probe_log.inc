/* Asian quota signability and offer probe logging. Included from foreign_asian_quota.inc. */

static int kbo_asian_quota_exception_available_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_team_foreign,
    uint32_t* out_team_asian,
    uint32_t* out_team_non_asian,
    uint32_t* out_team_effective,
    uint32_t* out_effective_after)
{
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);

    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    int candidate_is_asian = kbo_player_is_asian_quota_candidate(candidate);
    int exception_available = candidate_is_asian && asian_count == 0u;
    uint32_t effective_after = effective;
    if (!exception_available) {
        effective_after++;
    }

    if (out_team_foreign != NULL) { *out_team_foreign = foreign_count; }
    if (out_team_asian != NULL) { *out_team_asian = asian_count; }
    if (out_team_non_asian != NULL) { *out_team_non_asian = non_asian_count; }
    if (out_team_effective != NULL) { *out_team_effective = effective; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    return exception_available;
}

static void kbo_log_asian_quota_signability_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    int original_signability,
    uintptr_t caller_rva)
{
    if (player == NULL || player_id == 0u || team_id <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 240) {
        return;
    }

    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    uint32_t team_effective = 0u;
    uint32_t effective_after = 0u;
    int exception_available = kbo_asian_quota_exception_available_for_candidate(
        (uint32_t)team_id,
        player,
        &team_foreign,
        &team_asian,
        &team_non_asian,
        &team_effective,
        &effective_after);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);

    append_logf(
        "asian quota signability probe player=%u requester_team=%d nation=%u asian_candidate=%d exception_available=%d"
        " original=%d team_foreign_raw=%u team_asian=%u team_non_asian=%u team_effective_foreign=%u effective_after=%u current_team=%u active_team=%u caller_rva=0x%llx",
        player_id,
        team_id,
        nation_id,
        kbo_player_is_asian_quota_candidate(player),
        exception_available,
        original_signability,
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective,
        effective_after,
        current_team_id,
        active_team_id,
        (unsigned long long)caller_rva);
}

static void kbo_log_asian_quota_offer_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    uint8_t original_result,
    int32_t flag)
{
    if (player == NULL || player_id == 0u || team_id <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 160) {
        return;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_after = 0u;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    int exception_available = kbo_asian_quota_exception_available_for_candidate(
        (uint32_t)team_id,
        player,
        &foreign_count,
        &asian_count,
        &non_asian_count,
        &effective_count,
        &effective_after);

    append_logf(
        "asian quota offer probe player=%u requester_team=%d nation=%u asian_candidate=%d original=%u flag=%d"
        " exception_available=%d team_foreign_raw=%u team_asian=%u team_non_asian_foreign=%u team_effective_foreign=%u effective_after=%u current_team=%u active_team=%u current_league=%u today=%u",
        player_id,
        team_id,
        nation_id,
        kbo_nation_is_asian_quota_candidate(nation_id),
        (uint32_t)original_result,
        flag,
        exception_available,
        foreign_count,
        asian_count,
        non_asian_count,
        effective_count,
        effective_after,
        current_team_id,
        active_team_id,
        current_league_id,
        today);
}

