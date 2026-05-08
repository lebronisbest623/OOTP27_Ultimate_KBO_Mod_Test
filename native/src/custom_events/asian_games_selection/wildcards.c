#include "../custom_events_common.h"
#include "wildcards.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../core/core_current_date.h"
#include "../../core/core_save_paths.h"
#include "../../core/core_text_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../bootstrap/forward_declarations.h"
#include "../../team/team_lookup.h"

int kbo_asian_games_apply_wildcard_replacements(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int selected_count,
    int* wildcard_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    if (candidates == NULL || candidate_count <= 0 || selected_count <= 0 || wildcard_count == NULL
            || pitcher_count == NULL || catcher_count == NULL || infielder_count == NULL || outfielder_count == NULL) {
        return 0;
    }

    int replacements = 0;
    while (*wildcard_count < KBO_ASIAN_GAMES_MAX_WILDCARDS) {
        int best_candidate_index = -1;
        int best_roster_index = -1;
        int best_gain = -2147483647;
        int best_was_military_ready = 0;

        for (int pass = 0; pass < 2 && best_candidate_index < 0; pass++) {
            int allow_cross_bucket = pass != 0;
            for (int i = 0; i < candidate_count; i++) {
                KboAsianGamesCandidate* candidate = &candidates[i];
                if (candidate->selected || candidate->entry.age <= 24u) {
                    continue;
                }
                int candidate_military_ready = 1;
                uintptr_t player_ptr = candidate->entry.player_ptr;
                if (kbo_player_pointer_plausible(player_ptr)) {
                    uint8_t* player = (uint8_t*)player_ptr;
                    candidate_military_ready = player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u
                        && player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u;
                }

                for (int roster_index = 0; roster_index < selected_count; roster_index++) {
                    KboAsianGamesRosterEntry* current = &g_kbo_asian_games_roster[roster_index];
                    if (current->wildcard != 0u || current->age > 24u) {
                        continue;
                    }
                    int same_bucket = kbo_asian_games_roles_same_bucket(candidate->entry.role, current->role);
                    if (!same_bucket) {
                        if (!allow_cross_bucket) {
                            continue;
                        }
                        if (kbo_asian_games_role_is_pitcher(current->role) && *pitcher_count <= 9) {
                            continue;
                        }
                        if (kbo_asian_games_role_is_catcher(current->role) && *catcher_count <= 2) {
                            continue;
                        }
                        if (kbo_asian_games_role_is_infielder(current->role) && *infielder_count <= 5) {
                            continue;
                        }
                        if (kbo_asian_games_role_is_outfielder(current->role) && *outfielder_count <= 4) {
                            continue;
                        }
                    }

                    uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(current->original_team_id);
                    if (candidate->org_team_id != 0u
                            && old_org_id != candidate->org_team_id
                            && kbo_asian_games_roster_org_count(candidate->org_team_id, selected_count) >= KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS) {
                        continue;
                    }
                    int old_org_required = kbo_asian_games_find_org_index(required_orgs, required_org_count, old_org_id) >= 0;
                    if (old_org_required
                            && old_org_id != candidate->org_team_id
                            && kbo_asian_games_roster_org_count(old_org_id, selected_count) <= KBO_ASIAN_GAMES_TEAM_MIN_PLAYERS) {
                        continue;
                    }

                    int gain = candidate->entry.score - current->score;
                    if (best_candidate_index < 0
                            || (candidate_military_ready && !best_was_military_ready)
                            || (candidate_military_ready == best_was_military_ready && gain > best_gain)) {
                        best_candidate_index = i;
                        best_roster_index = roster_index;
                        best_gain = gain;
                        best_was_military_ready = candidate_military_ready;
                    }
                }
            }
        }

        if (best_candidate_index < 0 || best_roster_index < 0) {
            break;
        }

        KboAsianGamesRosterEntry old_entry = g_kbo_asian_games_roster[best_roster_index];
        KboAsianGamesCandidate* replacement = &candidates[best_candidate_index];
        replacement->entry.wildcard = 1u;
        g_kbo_asian_games_roster[best_roster_index] = replacement->entry;
        replacement->selected = 1u;
        if (!kbo_asian_games_roles_same_bucket(old_entry.role, replacement->entry.role)) {
            if (kbo_asian_games_role_is_pitcher(old_entry.role)) {
                (*pitcher_count)--;
            } else if (kbo_asian_games_role_is_catcher(old_entry.role)) {
                (*catcher_count)--;
            } else if (kbo_asian_games_role_is_infielder(old_entry.role)) {
                (*infielder_count)--;
            } else if (kbo_asian_games_role_is_outfielder(old_entry.role)) {
                (*outfielder_count)--;
            }
            if (kbo_asian_games_role_is_pitcher(replacement->entry.role)) {
                (*pitcher_count)++;
            } else if (kbo_asian_games_role_is_catcher(replacement->entry.role)) {
                (*catcher_count)++;
            } else if (kbo_asian_games_role_is_infielder(replacement->entry.role)) {
                (*infielder_count)++;
            } else if (kbo_asian_games_role_is_outfielder(replacement->entry.role)) {
                (*outfielder_count)++;
            }
        }
        (*wildcard_count)++;
        replacements++;

        append_logf(
            "KBO Asian Games wildcard replacement slot=%d old_player=%u old_age=%u old_role=%u old_score=%d new_player=%u new_age=%u new_role=%u new_score=%d gain=%d military_ready=%d same_bucket=%d",
            best_roster_index + 1,
            old_entry.player_id,
            (uint32_t)old_entry.age,
            (uint32_t)old_entry.role,
            old_entry.score,
            replacement->entry.player_id,
            (uint32_t)replacement->entry.age,
            (uint32_t)replacement->entry.role,
            replacement->entry.score,
            best_gain,
            best_was_military_ready,
            kbo_asian_games_roles_same_bucket(old_entry.role, replacement->entry.role));
    }

    return replacements;
}
