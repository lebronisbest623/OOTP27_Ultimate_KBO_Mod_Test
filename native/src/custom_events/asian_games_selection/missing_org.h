#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_MISSING_ORG_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SELECTION_MISSING_ORG_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_replace_for_missing_org(KboAsianGamesCandidate* candidates, int candidate_count, uint32_t missing_org_id, int selected_count, const uint32_t* required_orgs, int required_org_count);

#endif
