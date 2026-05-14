#ifndef KBOFIX_FOREIGN_INJURY_SCANNER_INTERNAL_H_
#define KBOFIX_FOREIGN_INJURY_SCANNER_INTERNAL_H_

#include "../internal/foreign_injury_internal.h"

int kbo_foreign_injury_player_matches_team(uint8_t* player, uint32_t team_id);
int kbo_foreign_injury_candidate_matches_slot(uint8_t* player, uint8_t slot_type);
int kbo_foreign_injury_player_has_baseball_position(uint8_t* player);
int kbo_foreign_injury_team_active_roster_contains_player(uint8_t* team, uint32_t player_id);
int kbo_foreign_injury_team_known_roster_contains_player(uint8_t* team, uint32_t player_id);
int kbo_foreign_injury_resolve_player_team_assignment(
    uint8_t* player,
    uint32_t player_id,
    uint32_t configured_league_id,
    uint32_t* out_team_id,
    uint32_t* out_league_id);
int kbo_foreign_injury_injured_player_returned_to_top_team(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured);
uint32_t kbo_foreign_injury_resolve_replacement_for_record(const KboForeignInjuryReplacement* rec);
int kbo_foreign_injury_restore_active_replacement_player(const KboForeignInjuryReplacement* rec, const char* source);
int kbo_foreign_injury_release_replacement_player(uint32_t team_id, uint32_t player_id, const char* source);

#endif
