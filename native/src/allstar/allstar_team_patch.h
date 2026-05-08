#ifndef KBOFIX_SRC_ALLSTAR_ALLSTAR_TEAM_PATCH_H_
#define KBOFIX_SRC_ALLSTAR_ALLSTAR_TEAM_PATCH_H_

#include <stdint.h>

int patch_kbo_allstar_team_names_for_league_id(uint32_t league_id, const char* source);
int patch_kbo_allstar_team_names_for_known_exhibition_teams(const char* source);
int patch_kbo_allstar_team_names_for_configured_league(const char* source);
uint8_t kbo_allstar_side_for_team(uint8_t* team, uint32_t league_year);
int ensure_kbo_allstar_team_ids(uintptr_t league_ptr, const char* source);

#endif
