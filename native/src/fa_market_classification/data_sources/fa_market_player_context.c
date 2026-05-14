#include "../internal/fa_market_data_internal.h"

#include "../../military_service/players/loans/military_active_loan.h"
#include "../../military_service/players/team_policy/military_service_team_policy.h"

uint32_t kbo_fa_market_get_team_league_id(uint32_t team_id)
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

int kbo_fa_market_team_belongs_to_league(uint32_t team_id, uint32_t league_id)
{
    if (team_id == 0u || league_id == 0u) {
        return 0;
    }
    return kbo_fa_market_get_team_league_id(team_id) == league_id;
}

static int kbo_fa_market_player_has_service_assignment(uint8_t* player, uint32_t player_id)
{
    if (player == NULL || player_id == 0u || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (kbo_team_id_is_military_service_team(current_team_id)) {
        return 1;
    }

    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    if (kbo_team_id_is_military_service_team(loan_team_id)) {
        return 1;
    }

    return find_active_kbo_military_loan_index(player_id) >= 0;
}

int kbo_fa_market_player_has_kbo_pro_context(uint8_t* player, uint32_t league_id)
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
    uint32_t original_team_id = kbo_fa_market_get_player_original_team_id(player);
    return kbo_fa_market_team_belongs_to_league(active_team_id, league_id)
        || kbo_fa_market_team_belongs_to_league(original_team_id, league_id);
}

int kbo_fa_market_player_is_candidate(uint8_t* player, uint32_t league_id)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];

    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (player_id == 0u
            || current_team_id != 0u
            || retired_flag != 0u
            || age < (uint16_t)policy->player_age_min
            || age > (uint16_t)policy->player_age_max) {
        return 0;
    }
    if (kbo_player_is_draft_pool_candidate(player)) {
        return 0;
    }
    if (!kbo_fa_market_player_has_kbo_pro_context(player, league_id)) {
        return 0;
    }
    if (kbo_fa_market_player_has_service_assignment(player, player_id)) {
        return 0;
    }
    return 1;
}
