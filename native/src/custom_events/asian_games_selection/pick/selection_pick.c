#include "../../runtime/common/custom_events_common.h"
#include "selection_pick.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_asian_games_try_select_candidate(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int index,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    int enforce_team_max)
{
    if (candidates == NULL || index < 0 || index >= candidate_count
            || selected_count == NULL || pitcher_count == NULL
            || catcher_count == NULL || infielder_count == NULL
            || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }
    const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
    if (*selected_count >= kbo_asian_games_policy_roster_size() || candidates[index].selected) {
        return 0;
    }

    KboAsianGamesRosterEntry* entry = &candidates[index].entry;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        if (*pitcher_count >= policy->pitcher_target) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        if (*catcher_count >= policy->catcher_target) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        if (*infielder_count >= policy->infielder_target) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_outfielder(entry->role)) {
        if (*outfielder_count >= policy->outfielder_target) {
            return 0;
        }
    } else {
        return 0;
    }

    int wildcard = kbo_asian_games_policy_is_wildcard_age(entry->age);
    if (wildcard && *wildcard_count >= policy->max_wildcards) {
        return 0;
    }
    if (enforce_team_max && candidates[index].org_team_id != 0u
            && kbo_asian_games_roster_org_count(candidates[index].org_team_id, *selected_count) >= policy->team_max_players) {
        return 0;
    }

    entry->wildcard = wildcard ? 1u : 0u;
    g_kbo_asian_games_roster[*selected_count] = *entry;
    candidates[index].selected = 1u;
    (*selected_count)++;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        (*pitcher_count)++;
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        (*catcher_count)++;
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        (*infielder_count)++;
    } else {
        (*outfielder_count)++;
    }
    if (wildcard) {
        (*wildcard_count)++;
    }
    return 1;
}

int kbo_asian_games_try_select_candidate_flex_position(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int index,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    int enforce_team_max)
{
    if (candidates == NULL || index < 0 || index >= candidate_count
            || selected_count == NULL || pitcher_count == NULL || catcher_count == NULL
            || infielder_count == NULL || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }
    const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
    if (*selected_count >= kbo_asian_games_policy_roster_size() || candidates[index].selected) {
        return 0;
    }

    KboAsianGamesRosterEntry* entry = &candidates[index].entry;
    if (!kbo_asian_games_role_is_pitcher(entry->role)
            && !kbo_asian_games_role_is_catcher(entry->role)
            && !kbo_asian_games_role_is_infielder(entry->role)
            && !kbo_asian_games_role_is_outfielder(entry->role)) {
        return 0;
    }

    int wildcard = kbo_asian_games_policy_is_wildcard_age(entry->age);
    if (wildcard && *wildcard_count >= policy->max_wildcards) {
        return 0;
    }
    if (enforce_team_max && candidates[index].org_team_id != 0u
            && kbo_asian_games_roster_org_count(candidates[index].org_team_id, *selected_count) >= policy->team_max_players) {
        return 0;
    }

    entry->wildcard = wildcard ? 1u : 0u;
    g_kbo_asian_games_roster[*selected_count] = *entry;
    candidates[index].selected = 1u;
    (*selected_count)++;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        (*pitcher_count)++;
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        (*catcher_count)++;
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        (*infielder_count)++;
    } else {
        (*outfielder_count)++;
    }
    if (wildcard) {
        (*wildcard_count)++;
    }
    return 1;
}
