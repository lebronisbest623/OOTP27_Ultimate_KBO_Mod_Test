#include "../../runtime/common/custom_events_common.h"
#include "missing_org.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_asian_games_replace_for_missing_org(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    uint32_t missing_org_id,
    int selected_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    if (candidates == NULL || candidate_count <= 0 || missing_org_id == 0u || selected_count <= 0) {
        return 0;
    }

    int best_candidate_index = -1;
    int best_roster_index = -1;
    int best_loss = 2147483647;
    const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();

    for (int i = 0; i < candidate_count; i++) {
        KboAsianGamesCandidate* candidate = &candidates[i];
        if (candidate->selected
                || candidate->org_team_id != missing_org_id
                || kbo_asian_games_policy_is_wildcard_age(candidate->entry.age)) {
            continue;
        }

        for (int roster_index = 0; roster_index < selected_count; roster_index++) {
            KboAsianGamesRosterEntry* current = &g_kbo_asian_games_roster[roster_index];
            if (current->wildcard != 0u || kbo_asian_games_policy_is_wildcard_age(current->age)) {
                continue;
            }
            if (!kbo_asian_games_roles_same_bucket(candidate->entry.role, current->role)) {
                continue;
            }

            uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(current->original_team_id);
            if (old_org_id == missing_org_id) {
                continue;
            }
            int old_org_required = kbo_asian_games_find_org_index(required_orgs, required_org_count, old_org_id) >= 0;
            if (old_org_required && kbo_asian_games_roster_org_count(old_org_id, selected_count) <= policy->team_min_players) {
                continue;
            }

            int loss = current->score - candidate->entry.score;
            if (best_candidate_index < 0 || loss < best_loss) {
                best_candidate_index = i;
                best_roster_index = roster_index;
                best_loss = loss;
            }
        }
    }

    if (best_candidate_index < 0 || best_roster_index < 0) {
        return 0;
    }

    KboAsianGamesRosterEntry old_entry = g_kbo_asian_games_roster[best_roster_index];
    g_kbo_asian_games_roster[best_roster_index] = candidates[best_candidate_index].entry;
    candidates[best_candidate_index].selected = 1u;

    append_logf(
        "KBO Asian Games team-min replacement org=%u slot=%d old_player=%u old_team=%u new_player=%u new_team=%u loss=%d",
        missing_org_id,
        best_roster_index + 1,
        old_entry.player_id,
        old_entry.original_team_id,
        candidates[best_candidate_index].entry.player_id,
        candidates[best_candidate_index].entry.original_team_id,
        best_loss);
    return 1;
}
