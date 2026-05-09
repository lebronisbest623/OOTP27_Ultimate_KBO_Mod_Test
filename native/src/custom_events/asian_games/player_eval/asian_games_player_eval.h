#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_PLAYER_EVAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_PLAYER_EVAL_H_

#include <stdint.h>
#include <windows.h>

#include "../state/asian_games_state.h"

int kbo_asian_games_candidate_compare_desc(const void* left, const void* right);
int32_t kbo_asian_games_player_score(uint8_t* player);
int kbo_asian_games_role_is_pitcher(uint8_t role);
int kbo_asian_games_role_is_catcher(uint8_t role);
int kbo_asian_games_role_is_infielder(uint8_t role);
int kbo_asian_games_role_is_outfielder(uint8_t role);
const char* kbo_asian_games_role_bucket_label(uint8_t role);
int kbo_asian_games_roles_same_bucket(uint8_t left, uint8_t right);
int kbo_asian_games_league_allowed(uint32_t league_id, const uint32_t* allowed_leagues, int allowed_count);
int kbo_ascii_contains_ignore_case(const char* text, const char* needle);
int kbo_asian_games_team_is_service_or_ulsan(uint8_t* team);
uint32_t kbo_asian_games_org_team_id_for_team(uint32_t team_id);
int kbo_asian_games_team_has_parent_if_affiliate(uint8_t* team, uint32_t main_league_id);
int kbo_asian_games_find_org_index(const uint32_t* org_ids, int org_count, uint32_t org_id);
int kbo_asian_games_roster_org_count(uint32_t org_id, int selected_count);
int kbo_asian_games_collect_required_orgs(uint32_t main_league_id, uint32_t* org_ids, int org_capacity);
uint32_t kbo_asian_games_final_date_for_year(uint32_t year);
int32_t kbo_asian_games_days_until_return(uint32_t event_yyyymmdd);

#endif
