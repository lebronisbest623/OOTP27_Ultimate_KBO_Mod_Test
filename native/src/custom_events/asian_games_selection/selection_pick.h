#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_SELECTION_PICK_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_SELECTION_PICK_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_try_select_candidate(KboAsianGamesCandidate* candidates, int candidate_count, int index, int* selected_count, int* pitcher_count, int* catcher_count, int* infielder_count, int* outfielder_count, int* wildcard_count, int enforce_team_max);
int kbo_asian_games_try_select_candidate_flex_position(KboAsianGamesCandidate* candidates, int candidate_count, int index, int* selected_count, int* pitcher_count, int* catcher_count, int* infielder_count, int* outfielder_count, int* wildcard_count, int enforce_team_max);

#endif
