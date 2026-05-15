#ifndef KBOFIX_SRC_FA_MARKET_INVESTIGATION_PROBE_DOMESTIC_FA_OFFER_PROBE_H_
#define KBOFIX_SRC_FA_MARKET_INVESTIGATION_PROBE_DOMESTIC_FA_OFFER_PROBE_H_

#include <stdint.h>

typedef struct KboDomesticFaOfferProbePlayer {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t loan_team_id;
    uint32_t draft_league_id;
    uint8_t retired;
    uint8_t position_group;
    uint8_t position_role;
    int32_t demand_salary;
    int32_t value_score;
} KboDomesticFaOfferProbePlayer;

int kbo_domestic_fa_offer_probe_read_player(
    uint8_t* player,
    KboDomesticFaOfferProbePlayer* out_player);
int kbo_domestic_fa_offer_probe_should_log_player(uint8_t* player);
int32_t kbo_domestic_fa_offer_probe_value_score(uint8_t* player);
void kbo_domestic_fa_offer_probe_log_candidate(
    uint8_t* player,
    uint32_t requester_team_id,
    int32_t before_index,
    int32_t after_index,
    uint32_t today);

#endif
