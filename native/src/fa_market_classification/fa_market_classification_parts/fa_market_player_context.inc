static uint32_t kbo_fa_market_resolve_league_id(uint32_t requested_league_id)
{
    (void)requested_league_id;

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id != 0u) {
        return league_id;
    }
    return kbo_resolve_kbo_league_id();
}

static uint32_t kbo_fa_market_get_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static int kbo_fa_market_team_belongs_to_league(uint32_t team_id, uint32_t league_id)
{
    if (team_id == 0u || league_id == 0u) {
        return 0;
    }
    return kbo_fa_market_get_team_league_id(team_id) == league_id;
}

static int kbo_fa_market_player_has_kbo_pro_context(uint8_t* player, uint32_t league_id)
{
    if (player == NULL || league_id == 0u) {
        return 1;
    }

    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (current_league_id == league_id || draft_league_id == league_id) {
        return 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == 0u
            && kbo_player_has_nonzero_evaluation(player)) {
        return 1;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = kbo_get_player_original_team_id(player);
    return kbo_fa_market_team_belongs_to_league(active_team_id, league_id)
        || kbo_fa_market_team_belongs_to_league(original_team_id, league_id);
}

static int kbo_fa_market_player_is_candidate(uint8_t* player, uint32_t league_id)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];

    if (player_id == 0u || current_team_id != 0u || retired_flag != 0u || age < 16u || age > 60u) {
        return 0;
    }
    if (kbo_player_is_draft_pool_candidate(player)) {
        return 0;
    }
    return kbo_fa_market_player_has_kbo_pro_context(player, league_id);
}

static int kbo_fa_market_row_is_undrafted_domestic(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->generation_context != 0u
            || row->generation_grade == 0u) {
        return 0;
    }

    if (row->draft_league_id == 201u && row->draft_subtype == 1u && row->age <= 25u) {
        return 1;
    }
    if (row->draft_league_id == 200u && row->age <= 20u) {
        return 1;
    }
    return 0;
}

static int kbo_fa_market_row_is_independent_league_fa(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->original_team_id == 0u) {
        return 0;
    }

    if (kbo_fa_market_row_is_undrafted_domestic(row)) {
        return 0;
    }

    uint32_t original_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
    if (original_league_id == KBO_FA_MARKET_INDEPENDENT_LEAGUE_ID) {
        return 1;
    }
    return original_league_id == 0u && row->draft_league_id == KBO_FA_MARKET_INDEPENDENT_LEAGUE_ID;
}
