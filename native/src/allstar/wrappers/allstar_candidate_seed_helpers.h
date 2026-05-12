#ifndef KBOFIX_SRC_ALLSTAR_WRAPPERS_ALLSTAR_CANDIDATE_SEED_HELPERS_H_
#define KBOFIX_SRC_ALLSTAR_WRAPPERS_ALLSTAR_CANDIDATE_SEED_HELPERS_H_

#include <stdint.h>
#include <windows.h>

int kbo_allstar_candidate_seed_is_exhibition_team(uint8_t* team);
uint8_t kbo_allstar_candidate_player_side(
    uint8_t* player,
    uint32_t league_id,
    uint32_t league_year,
    uint32_t* out_team_id);

#endif
