#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_WILDCARDS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_WILDCARDS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_apply_wildcard_replacements(KboAsianGamesCandidate* candidates, int candidate_count, int selected_count, int* wildcard_count, int* pitcher_count, int* catcher_count, int* infielder_count, int* outfielder_count, const uint32_t* required_orgs, int required_org_count);

#endif
