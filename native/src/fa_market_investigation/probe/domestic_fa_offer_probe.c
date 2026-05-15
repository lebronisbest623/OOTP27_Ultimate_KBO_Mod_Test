#include "domestic_fa_offer_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"

#define KBO_DOMESTIC_FA_OFFER_PROBE_SCORE_MIN 85000

int32_t kbo_domestic_fa_offer_probe_value_score(uint8_t* player)
{
    return kbo_foreign_waiver_value_score(player);
}

int kbo_domestic_fa_offer_probe_read_player(
    uint8_t* player,
    KboDomesticFaOfferProbePlayer* out_player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (out_player == NULL) {
        return 1;
    }

    out_player->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out_player->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out_player->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out_player->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out_player->original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    out_player->loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    out_player->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    out_player->retired = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
    out_player->position_group = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
    out_player->position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    out_player->demand_salary = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    out_player->value_score = kbo_domestic_fa_offer_probe_value_score(player);
    return 1;
}

int kbo_domestic_fa_offer_probe_should_log_player(uint8_t* player)
{
    KboDomesticFaOfferProbePlayer probe_player;
    if (!kbo_domestic_fa_offer_probe_read_player(player, &probe_player)) {
        return 0;
    }

    return probe_player.nation_id == OOTP27_KBO_KOREA_NATION_ID
        && probe_player.retired == 0u
        && probe_player.current_team_id == 0u
        && probe_player.active_team_id == 0u
        && probe_player.loan_team_id == 0u
        && probe_player.draft_league_id == 0u
        && probe_player.value_score >= KBO_DOMESTIC_FA_OFFER_PROBE_SCORE_MIN;
}

void kbo_domestic_fa_offer_probe_log_candidate(
    uint8_t* player,
    uint32_t requester_team_id,
    int32_t before_index,
    int32_t after_index,
    uint32_t today)
{
    KboDomesticFaOfferProbePlayer probe_player;
    if (!kbo_domestic_fa_offer_probe_should_log_player(player)
            || !kbo_domestic_fa_offer_probe_read_player(player, &probe_player)) {
        return;
    }

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 800) {
        return;
    }

    kbo_log_runtimef(
        "domestic FA AI status candidate probe #%ld player=%u requester_team=%u inserted=%d index=%d next=%d today=%u demand=%d score=%d current=%u active=%u original=%u draft=%u pos=%u/%u",
        slot,
        probe_player.player_id,
        requester_team_id,
        after_index != before_index ? 1 : 0,
        before_index,
        after_index,
        today,
        probe_player.demand_salary,
        probe_player.value_score,
        probe_player.current_team_id,
        probe_player.active_team_id,
        probe_player.original_team_id,
        probe_player.draft_league_id,
        (uint32_t)probe_player.position_group,
        (uint32_t)probe_player.position_role);
}
