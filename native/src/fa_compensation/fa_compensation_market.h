#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_MARKET_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_MARKET_H_

#include <stdint.h>

uint32_t kbo_fa_compensation_original_team_for_player(uint32_t fa_player_id);
uint32_t kbo_fa_compensation_signing_team_for_player(uint32_t fa_player_id);

int kbo_fa_compensation_build_market_row(
    uint8_t* player,
    struct KboFaMarketClassification* row,
    uint32_t league_id,
    uint32_t current_year,
    uint32_t today,
    const struct KboFaRules* rules);

#endif
