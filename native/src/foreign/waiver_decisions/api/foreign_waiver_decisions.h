#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_DECISIONS_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_DECISIONS_H_

#include <stdint.h>

uint32_t kbo_get_player_original_team_id(uint8_t* player);

int kbo_retain_foreign_player_rights(
    uint8_t* player,
    uint8_t* retaining_team,
    uint32_t fallback_league_id,
    uint32_t player_id,
    uint32_t team_id);

int kbo_append_foreign_waiver_decision_record(
    const char* source,
    const char* action,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed);

int kbo_foreign_waiver_decision_exists(uint32_t window_end, uint32_t team_id, uint32_t player_id);

#endif
