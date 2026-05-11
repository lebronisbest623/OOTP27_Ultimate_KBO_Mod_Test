#include "../../runtime/common/custom_events_common.h"
#include "select_roster.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/optimizer/kbo_optimizer.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../asian_games/roster/asian_games_roster_store.h"
#include "../missing_org/missing_org.h"
#include "../pick/selection_pick.h"
#include "../wildcards/wildcards.h"

static const char* kbo_asian_games_role_bucket_code(uint8_t role)
{
    if (kbo_asian_games_role_is_pitcher(role)) {
        return "P";
    }
    if (kbo_asian_games_role_is_catcher(role)) {
        return "C";
    }
    if (kbo_asian_games_role_is_infielder(role)) {
        return "IF";
    }
    if (kbo_asian_games_role_is_outfielder(role)) {
        return "OF";
    }
    return "UNK";
}

static int kbo_asian_games_write_ortools_request(
    const char* path,
    const KboAsianGamesCandidate* candidates,
    int candidate_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    fputs("player_id,score,age,role,role_bucket,org_team_id,required_org\r\n", file);
    for (int i = 0; i < candidate_count; i++) {
        const KboAsianGamesCandidate* candidate = &candidates[i];
        uint32_t player_id = candidate->entry.player_id;
        if (player_id == 0u) {
            continue;
        }
        int required = kbo_asian_games_find_org_index(
            required_orgs,
            required_org_count,
            candidate->org_team_id) >= 0;
        fprintf(
            file,
            "%u,%d,%u,%u,%s,%u,%d\r\n",
            player_id,
            candidate->entry.score,
            (uint32_t)candidate->entry.age,
            (uint32_t)candidate->entry.role,
            kbo_asian_games_role_bucket_code(candidate->entry.role),
            candidate->org_team_id,
            required);
    }
    fclose(file);
    return 1;
}

static int kbo_asian_games_find_candidate_by_player_id(
    const KboAsianGamesCandidate* candidates,
    int candidate_count,
    uint32_t player_id)
{
    if (candidates == NULL || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].entry.player_id == player_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_asian_games_apply_ortools_result(
    const char* result_path,
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count)
{
    if (result_path == NULL || candidates == NULL || selected_count == NULL
            || pitcher_count == NULL || catcher_count == NULL
            || infielder_count == NULL || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }

    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }
    char line[256] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    uint32_t selected_ids[KBO_ASIAN_GAMES_ROSTER_SIZE] = {0};
    int id_count = 0;
    while (id_count < KBO_ASIAN_GAMES_ROSTER_SIZE && fgets(line, sizeof(line), file) != NULL) {
        uint32_t player_id = (uint32_t)strtoul(line, NULL, 10);
        if (player_id == 0u) {
            continue;
        }
        int duplicate = 0;
        for (int i = 0; i < id_count; i++) {
            if (selected_ids[i] == player_id) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            selected_ids[id_count++] = player_id;
        }
    }
    fclose(file);

    int target_count = candidate_count < KBO_ASIAN_GAMES_ROSTER_SIZE
        ? candidate_count
        : KBO_ASIAN_GAMES_ROSTER_SIZE;
    if (id_count < target_count) {
        return 0;
    }

    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    for (int i = 0; i < candidate_count; i++) {
        candidates[i].selected = 0u;
    }
    *selected_count = 0;
    *pitcher_count = 0;
    *catcher_count = 0;
    *infielder_count = 0;
    *outfielder_count = 0;
    *wildcard_count = 0;

    for (int i = 0; i < id_count && *selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        int index = kbo_asian_games_find_candidate_by_player_id(candidates, candidate_count, selected_ids[i]);
        if (index < 0) {
            return 0;
        }
        KboAsianGamesRosterEntry entry = candidates[index].entry;
        entry.wildcard = entry.age > 24u ? 1u : 0u;
        g_kbo_asian_games_roster[*selected_count] = entry;
        candidates[index].selected = 1u;
        (*selected_count)++;
        if (kbo_asian_games_role_is_pitcher(entry.role)) {
            (*pitcher_count)++;
        } else if (kbo_asian_games_role_is_catcher(entry.role)) {
            (*catcher_count)++;
        } else if (kbo_asian_games_role_is_infielder(entry.role)) {
            (*infielder_count)++;
        } else if (kbo_asian_games_role_is_outfielder(entry.role)) {
            (*outfielder_count)++;
        }
        if (entry.wildcard) {
            (*wildcard_count)++;
        }
    }
    return *selected_count;
}

static int kbo_select_asian_games_roster_ortools(
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
    const char* source)
{
    if (read_kbo_localappdata_flag_file("disable_asian_games_ortools.txt")
            || candidates == NULL
            || candidate_count <= 0) {
        return 0;
    }

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("asian_games_roster_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("asian_games_roster_ortools_result.csv", result_path, sizeof(result_path))) {
        return 0;
    }
    if (!kbo_asian_games_write_ortools_request(
            request_path,
            candidates,
            candidate_count,
            required_orgs,
            required_org_count)) {
        return 0;
    }
    if (!kbo_optimizer_run_mode("asian_games_roster", request_path, result_path, 8000u)) {
        return 0;
    }
    int applied = kbo_asian_games_apply_ortools_result(
        result_path,
        candidates,
        candidate_count,
        selected_count,
        pitcher_count,
        catcher_count,
        infielder_count,
        outfielder_count,
        wildcard_count);
    if (applied > 0) {
        append_logf(
            "KBO Asian Games OR-Tools roster selected source=%s candidates=%d selected=%d required_orgs=%d pitchers=%d catchers=%d infielders=%d outfielders=%d wildcards=%d",
            source != NULL ? source : "",
            candidate_count,
            applied,
            required_org_count,
            *pitcher_count,
            *catcher_count,
            *infielder_count,
            *outfielder_count,
            *wildcard_count);
    }
    return applied;
}

int kbo_select_asian_games_roster(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t year = event_yyyymmdd / 10000u;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=player_vector_unavailable",
            source != NULL ? source : "",
            year);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    if (kbo_league_id == 200u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = 201u;
    }

    KboAsianGamesCandidate* candidates = (KboAsianGamesCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAsianGamesCandidate) * KBO_ASIAN_GAMES_MAX_CANDIDATES);
    if (candidates == NULL) {
        append_logf(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=alloc_failed",
            source != NULL ? source : "",
            year);
        return 0;
    }

    int candidate_count = 0;
    int scanned = 0;
    int rejected_nation = 0;
    int rejected_status = 0;
    int rejected_team = 0;
    int rejected_league = 0;
    int rejected_service_team = 0;
    int rejected_parentless_affiliate = 0;
    uint32_t main_league_id = allowed_league_count > 0 ? allowed_leagues[0] : 0u;
    for (int32_t i = 0; i < player_count && candidate_count < KBO_ASIAN_GAMES_MAX_CANDIDATES; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_nation++;
            continue;
        }
        if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u) {
            rejected_status++;
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            rejected_league++;
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        uint32_t team_nation_id = current_team != NULL
            ? *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET)
            : 0u;
        if (current_team_id == 0u || current_league_id == 0u
                || current_team == NULL
                || team_nation_id != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_team++;
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }
        if (!kbo_asian_games_team_has_parent_if_affiliate(current_team, main_league_id)) {
            rejected_parentless_affiliate++;
            continue;
        }

        KboAsianGamesCandidate* candidate = &candidates[candidate_count++];
        candidate->entry.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        candidate->entry.original_team_id = current_team_id;
        candidate->entry.original_league_id = current_league_id;
        candidate->entry.departure_date = 0u;
        candidate->entry.return_date = 0u;
        candidate->entry.age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        candidate->entry.role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        candidate->entry.wildcard = 0u;
        candidate->entry.old_restricted = 0u;
        candidate->entry.old_secondary_restricted = 0u;
        candidate->entry.old_injury_active = 0u;
        candidate->entry.old_injury_days_left = 0;
        candidate->entry.departed = 0u;
        candidate->entry.returned = 0u;
        candidate->entry.exempted = 0u;
        candidate->entry.score = kbo_asian_games_player_score(player);
        candidate->entry.player_ptr = player_ptr;
        candidate->org_team_id = kbo_asian_games_org_team_id_for_team(current_team_id);
    }

    qsort(candidates, (size_t)candidate_count, sizeof(KboAsianGamesCandidate), kbo_asian_games_candidate_compare_desc);
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = year;

    uint32_t required_orgs[KBO_ASIAN_GAMES_MAX_ORGS] = {0};
    int required_org_count = kbo_asian_games_collect_required_orgs(
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        required_orgs,
        KBO_ASIAN_GAMES_MAX_ORGS);

    int selected_count = 0;
    int pitcher_count = 0;
    int catcher_count = 0;
    int infielder_count = 0;
    int outfielder_count = 0;
    int wildcard_count = 0;
    int wildcard_fills = 0;
    int flex_fills = 0;
    int required_org_repairs = 0;
    int wildcard_replacements = 0;
    int used_ortools = 0;

    int required_org_initial_selected = 0;
    if (kbo_select_asian_games_roster_ortools(
            candidates,
            candidate_count,
            required_orgs,
            required_org_count,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            source) > 0) {
        used_ortools = 1;
    }

    for (int org_index = 0; !used_ortools && org_index < required_org_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; org_index++) {
        int best_index = -1;
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].selected
                    || candidates[i].org_team_id != required_orgs[org_index]
                    || candidates[i].entry.age > 24u) {
                continue;
            }
            best_index = i;
            if (kbo_asian_games_try_select_candidate(
                    candidates,
                    candidate_count,
                    i,
                    &selected_count,
                    &pitcher_count,
                    &catcher_count,
                    &infielder_count,
                    &outfielder_count,
                    &wildcard_count,
                    1)) {
                required_org_initial_selected++;
                break;
            }
        }
        if (best_index >= 0 && kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) == 0) {
            append_logf(
                "KBO Asian Games team-min initial select deferred org=%u best_player=%u best_role=%u reason=position_full_or_team_max",
                required_orgs[org_index],
                candidates[best_index].entry.player_id,
                (uint32_t)candidates[best_index].entry.role);
        }
    }

    for (int i = 0; !used_ortools && i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        if (candidates[i].entry.age > 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
    }

    for (int i = 0; !used_ortools && i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        int before_selected = selected_count;
        int before_wildcards = wildcard_count;
        if (candidates[i].entry.age <= 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
        if (selected_count > before_selected && wildcard_count > before_wildcards) {
            wildcard_fills++;
        }
    }

    for (int i = 0; !used_ortools && i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        int before_selected = selected_count;
        if (candidates[i].entry.age > 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate_flex_position(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
        if (selected_count > before_selected) {
            flex_fills++;
        }
    }

    for (int i = 0; !used_ortools && i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        int before_selected = selected_count;
        if (candidates[i].entry.age <= 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate_flex_position(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
        if (selected_count > before_selected) {
            flex_fills++;
        }
    }

    for (int org_index = 0; !used_ortools && org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) > 0) {
            continue;
        }
        if (kbo_asian_games_replace_for_missing_org(
                candidates,
                candidate_count,
                required_orgs[org_index],
                selected_count,
                required_orgs,
                required_org_count)) {
            required_org_repairs++;
        }
    }

    int required_org_missing = 0;
    for (int org_index = 0; org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) <= 0) {
            required_org_missing++;
        }
    }

    if (!used_ortools) {
        wildcard_replacements = kbo_asian_games_apply_wildcard_replacements(
            candidates,
            candidate_count,
            selected_count,
            &wildcard_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            required_orgs,
            required_org_count);
    }

    g_kbo_asian_games_roster_count = selected_count;
    append_logf(
        "KBO Asian Games roster selected source=%s year=%u method=%s scanned=%d candidates=%d selected=%d pitchers=%d catchers=%d infielders=%d outfielders=%d wildcards=%d wildcard_fills=%d flex_fills=%d wildcard_replacements=%d required_orgs=%d required_initial=%d required_repairs=%d required_missing=%d team_max=%d allowed_leagues=%u/%u vector_offset=0x%x rejected_nation=%d rejected_status=%d rejected_league=%d rejected_team=%d rejected_service_team=%d rejected_parentless_affiliate=%d",
        source != NULL ? source : "",
        year,
        used_ortools ? "ortools" : "greedy",
        scanned,
        candidate_count,
        selected_count,
        pitcher_count,
        catcher_count,
        infielder_count,
        outfielder_count,
        wildcard_count,
        wildcard_fills,
        flex_fills,
        wildcard_replacements,
        required_org_count,
        required_org_initial_selected,
        required_org_repairs,
        required_org_missing,
        KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS,
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        allowed_league_count > 1 ? allowed_leagues[1] : 0u,
        vector_offset,
        rejected_nation,
        rejected_status,
        rejected_league,
        rejected_team,
        rejected_service_team,
        rejected_parentless_affiliate);
    for (int i = 0; i < selected_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        append_logf(
            "KBO Asian Games selected #%02d player_id=%u team=%u org=%u league=%u age=%u role=%u bucket=%s wildcard=%u score=%d",
            i + 1,
            entry->player_id,
            entry->original_team_id,
            kbo_asian_games_org_team_id_for_team(entry->original_team_id),
            entry->original_league_id,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            (uint32_t)entry->wildcard,
            entry->score);
    }

    HeapFree(GetProcessHeap(), 0, candidates);
    kbo_save_asian_games_roster_csv(source);
    return selected_count;
}
