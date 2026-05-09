#ifndef KBOFIX_SRC_TEAM_TEAM_LOOKUP_H_
#define KBOFIX_SRC_TEAM_TEAM_LOOKUP_H_

#include <stdint.h>

int kbo_player_pointer_plausible(uintptr_t player_ptr);
int kbo_player_is_draft_pool_candidate(uint8_t* player);
int kbo_player_has_nonzero_evaluation(uint8_t* player);
int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset);
uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted);
uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted);

#endif
