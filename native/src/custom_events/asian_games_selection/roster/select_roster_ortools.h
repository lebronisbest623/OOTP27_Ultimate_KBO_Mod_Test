#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_ROSTER_SELECT_ROSTER_ORTOOLS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_ROSTER_SELECT_ROSTER_ORTOOLS_H_

#include "../../asian_games/state/asian_games_state.h"

int kbo_select_asian_games_roster_ortools(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    const uint32_t* required_orgs,
    int required_org_count,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    const char* source);

#endif
