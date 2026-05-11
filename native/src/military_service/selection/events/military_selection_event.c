#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/optimizer/kbo_optimizer.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/assignment/team_assignment.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../players/loans/military_active_loan.h"
#include "../draft/military_draft_queue.h"
#include "../../players/state/military_player_state.h"
#include "military_selection_event.h"
#include "../news/military_selection_news.h"
#include "../../seed/parse/military_service_seed_parse.h"
#include "../../runtime/days_tick/military_service_tick.h"

/* Military custom-event selection routing. */

static int kbo_count_players_on_team_with_military_days(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }
    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == team_id
                && kbo_military_effective_days_left(player) > 0) {
            count++;
        }
    }
    return count;
}

static int16_t kbo_military_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(int16_t))) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

static int kbo_military_draft_candidate_score(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return INT32_MIN;
    }
    int32_t overall = kbo_military_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_military_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_military_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_military_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int score = talent * 55 + overall * 25 + ratings * 10 + career * 10;

    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (age >= 21 && age <= 26) {
        score += 300 - (int)(age - 21u) * 20;
    } else if (age < 21) {
        score += 120 - (int)(21u - age) * 10;
    } else {
        score += 120 - (int)(age - 26u) * 15;
    }
    score += (int)(*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) % 97u);
    return score;
}

static int kbo_route_military_candidate_to_sang(
    KboMilitaryDraftCandidate* candidate,
    uint8_t* player,
    uint8_t* sang,
    uint32_t sang_id,
    uint32_t sang_league_id,
    int score,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    int routed,
    uint16_t entry_year,
    const char* source)
{
    if (candidate == NULL || player == NULL || sang == NULL) {
        return 0;
    }

    uint32_t original_team_id = candidate->original_team_id != 0u
        ? candidate->original_team_id
        : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t original_league_id = candidate->original_league_id != 0u
        ? candidate->original_league_id
        : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (original_team_id == 0u || original_team_id == sang_id) {
        kbo_military_resolve_original_team(
            player,
            sang_id,
            sang_id,
            0u,
            &original_team_id,
            &original_league_id);
    }

    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_internal(
        player,
        sang,
        sang_league_id,
        0,
        &called_pre_change,
        &called_register,
        &called_attach);
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = original_team_id;
    if (original_league_id != 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = original_league_id;
    }
    register_active_kbo_military_loan(
        candidate->player_id,
        (uintptr_t)player,
        original_team_id,
        original_league_id,
        sang_id,
        sang_league_id);
    kbo_clear_military_unavailable_flags(player);
    candidate->selected = 1u;
    if (news_entries != NULL && routed < max_news_entries) {
        KboMilitarySelectionNewsEntry* news_entry = &news_entries[routed];
        news_entry->player_id = candidate->player_id;
        news_entry->original_team_id = original_team_id;
        news_entry->score = score;
        news_entry->player_ptr = (uintptr_t)player;
    }
    append_logf(
        "KBO military selection drafted source=%s year=%u player_id=%u original_team=%u service_team=%u score=%d pre=%d register=%d attach=%d",
        source != NULL ? source : "",
        entry_year,
        candidate->player_id,
        original_team_id,
        sang_id,
        score,
        called_pre_change,
        called_register,
        called_attach);
    return 1;
}

static int kbo_military_write_ortools_request(
    const char* path,
    uint16_t entry_year,
    uint32_t sang_id,
    int slots,
    int* out_considered)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    fputs("player_id,score,age,role,original_team_id,slots\r\n", file);
    int considered = 0;
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u || candidate->entry_year != entry_year || candidate->selected != 0u) {
            continue;
        }
        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            candidate->player_ptr = player_ptr;
        }
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                || kbo_military_effective_days_left(player) <= 0
                || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == sang_id) {
            continue;
        }
        int score = kbo_military_draft_candidate_score(player);
        fprintf(
            file,
            "%u,%d,%u,%u,%u,%d\r\n",
            candidate->player_id,
            score,
            (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)),
            (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
            candidate->original_team_id,
            slots);
        considered++;
    }
    fclose(file);
    if (out_considered != NULL) {
        *out_considered = considered;
    }
    return considered > 0;
}

static int kbo_military_read_ortools_result(
    const char* path,
    uint32_t* selected_player_ids,
    int selected_capacity)
{
    if (path == NULL || selected_player_ids == NULL || selected_capacity <= 0) {
        return 0;
    }
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    char line[256] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }
    int count = 0;
    while (count < selected_capacity && fgets(line, sizeof(line), file) != NULL) {
        uint32_t player_id = (uint32_t)strtoul(line, NULL, 10);
        if (player_id != 0u) {
            selected_player_ids[count++] = player_id;
        }
    }
    fclose(file);
    return count;
}

static KboMilitaryDraftCandidate* kbo_military_find_unselected_candidate(
    uint32_t player_id,
    uint16_t entry_year)
{
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == player_id && candidate->entry_year == entry_year && candidate->selected == 0u) {
            return candidate;
        }
    }
    return NULL;
}

static int kbo_route_queued_military_draft_candidates_ortools(
    uint16_t entry_year,
    uint8_t* sang,
    uint32_t sang_id,
    uint32_t sang_league_id,
    int slots,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    int* out_considered,
    const char* source)
{
    if (read_kbo_localappdata_flag_file("disable_military_ortools.txt")
            || entry_year == 0u || sang == NULL || slots <= 0) {
        return 0;
    }
    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("military_selection_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("military_selection_ortools_result.csv", result_path, sizeof(result_path))) {
        return 0;
    }
    int considered = 0;
    if (!kbo_military_write_ortools_request(request_path, entry_year, sang_id, slots, &considered)) {
        if (out_considered != NULL) {
            *out_considered = considered;
        }
        return 0;
    }
    if (out_considered != NULL) {
        *out_considered = considered;
    }
    if (!kbo_optimizer_run_mode("military_selection", request_path, result_path, 8000u)) {
        return 0;
    }
    uint32_t selected_player_ids[64] = {0};
    int selected_count = kbo_military_read_ortools_result(
        result_path,
        selected_player_ids,
        (int)(sizeof(selected_player_ids) / sizeof(selected_player_ids[0])));
    if (selected_count <= 0) {
        return 0;
    }

    int routed = 0;
    for (int i = 0; i < selected_count && routed < slots; i++) {
        KboMilitaryDraftCandidate* candidate = kbo_military_find_unselected_candidate(
            selected_player_ids[i],
            entry_year);
        if (candidate == NULL) {
            continue;
        }
        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            candidate->player_ptr = player_ptr;
        }
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                || kbo_military_effective_days_left(player) <= 0
                || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == sang_id) {
            continue;
        }
        int score = kbo_military_draft_candidate_score(player);
        routed += kbo_route_military_candidate_to_sang(
            candidate,
            player,
            sang,
            sang_id,
            sang_league_id,
            score,
            news_entries,
            max_news_entries,
            routed,
            entry_year,
            source) ? 1 : 0;
    }
    if (routed > 0) {
        append_logf(
            "KBO military OR-Tools selection processed source=%s year=%u considered=%d selected=%d routed=%d slots=%d",
            source != NULL ? source : "",
            entry_year,
            considered,
            selected_count,
            routed,
            slots);
    }
    return routed;
}

static int kbo_route_queued_military_draft_candidates(
    uint16_t entry_year,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    const char* source)
{
    if (entry_year == 0u) {
        return 0;
    }
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    if (sang == NULL || !memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        append_logf("KBO military selection skipped source=%s year=%u reason=no_sang", source != NULL ? source : "", entry_year);
        return 0;
    }
    uint32_t sang_id = *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t sang_league_id = *(uint32_t*)(sang + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (sang_id == 0u || sang_league_id == 0u) {
        append_logf("KBO military selection skipped source=%s year=%u reason=bad_sang_ids team=%u league=%u", source != NULL ? source : "", entry_year, sang_id, sang_league_id);
        return 0;
    }

    enum { KBO_MILITARY_SANG_CAPACITY = 35, KBO_MILITARY_SANG_ANNUAL_INTAKE = 18 };
    int on_roster = kbo_count_players_on_team_with_military_days(sang_id);
    int slots = KBO_MILITARY_SANG_CAPACITY - on_roster;
    if (slots > KBO_MILITARY_SANG_ANNUAL_INTAKE) {
        slots = KBO_MILITARY_SANG_ANNUAL_INTAKE;
    }
    if (slots <= 0) {
        append_logf("KBO military selection skipped source=%s year=%u reason=sang_full active=%d", source != NULL ? source : "", entry_year, on_roster);
        return 0;
    }

    int routed = 0;
    int considered = 0;
    routed = kbo_route_queued_military_draft_candidates_ortools(
        entry_year,
        sang,
        sang_id,
        sang_league_id,
        slots,
        news_entries,
        max_news_entries,
        &considered,
        source);
    if (routed > 0) {
        append_logf(
            "KBO military selection processed source=%s year=%u method=ortools queued=%d considered=%d routed=%d active_before=%d slots=%d",
            source != NULL ? source : "",
            entry_year,
            kbo_count_military_draft_candidates_for_year(entry_year),
            considered,
            routed,
            on_roster,
            slots);
        return routed;
    }

    for (;;) {
        int best_index = -1;
        int best_score = INT32_MIN;
        LONG count = g_kbo_military_draft_candidate_count;
        if (count < 0) { count = 0; }
        if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
        for (LONG i = 0; i < count; i++) {
            KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
            if (candidate->player_id == 0u || candidate->entry_year != entry_year || candidate->selected != 0u) {
                continue;
            }
            considered++;
            uintptr_t player_ptr = candidate->player_ptr;
            if (!kbo_player_pointer_plausible(player_ptr)) {
                player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
                candidate->player_ptr = player_ptr;
            }
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                    || kbo_military_effective_days_left(player) <= 0
                    || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == sang_id) {
                continue;
            }
            int score = kbo_military_draft_candidate_score(player);
            if (score > best_score) {
                best_score = score;
                best_index = (int)i;
            }
        }
        if (best_index < 0 || routed >= slots) {
            break;
        }

        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[best_index];
        uint8_t* player = (uint8_t*)candidate->player_ptr;
        routed += kbo_route_military_candidate_to_sang(
            candidate,
            player,
            sang,
            sang_id,
            sang_league_id,
            best_score,
            news_entries,
            max_news_entries,
            routed,
            entry_year,
            source) ? 1 : 0;
    }

    append_logf(
        "KBO military selection processed source=%s year=%u method=greedy queued=%d considered=%d routed=%d active_before=%d slots=%d",
        source != NULL ? source : "",
        entry_year,
        kbo_count_military_draft_candidates_for_year(entry_year),
        considered,
        routed,
        on_roster,
        slots);
    return routed;
}

int kbo_refresh_military_selection_candidates_from_memory(
    uint16_t entry_year,
    uint32_t sang_id,
    const char* source)
{
    if (entry_year == 0u || sang_id == 0u) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf(
            "KBO military selection candidate refresh skipped source=%s year=%u reason=player_vector_unavailable",
            source != NULL ? source : "",
            entry_year);
        return 0;
    }

    int scanned = 0;
    int eligible = 0;
    int queued = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        int32_t days_left = kbo_military_effective_days_left(player);
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                || days_left < KBO_MILITARY_SERVICE_DAYS - 90) {
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (current_team_id == 0u || current_team_id == sang_id) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        if (!kbo_military_resolve_original_team(
                player,
                sang_id,
                sang_id,
                0u,
                &original_team_id,
                &original_league_id)) {
            original_team_id = current_team_id;
            original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        }
        eligible++;
        queued += kbo_queue_military_draft_candidate(
            player_ptr,
            player_id,
            entry_year,
            original_team_id,
            original_league_id,
            "military_selection_memory_refresh") ? 1 : 0;
    }

    append_logf(
        "KBO military selection candidate refresh source=%s year=%u scanned=%d eligible=%d queued_or_refreshed=%d total_queued=%d vector_off=0x%x",
        source != NULL ? source : "",
        entry_year,
        scanned,
        eligible,
        queued,
        kbo_count_military_draft_candidates_for_year(entry_year),
        vector_offset);
    return queued;
}

int run_kbo_custom_military_event(
    uintptr_t event_ptr,
    const char* event_name,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source)
{
    (void)event_ptr;
    int seeded = 0;
    int returned = kbo_tick_military_service_days("military_selection_event", &seeded);
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint32_t sang_id = sang != NULL && memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    int refreshed = kbo_refresh_military_selection_candidates_from_memory(
        (uint16_t)event_year,
        sang_id,
        source);
    KboMilitarySelectionNewsEntry news_entries[24] = {0};
    int routed = kbo_route_queued_military_draft_candidates(
        (uint16_t)event_year,
        news_entries,
        (int)(sizeof(news_entries) / sizeof(news_entries[0])),
        source);
    uint32_t event_yyyymmdd =
        event_year * 10000u + event_month * 100u + event_day;
    int news_created = 0;
    if (routed > 0) {
        news_created = kbo_emit_military_selection_news(
            event_yyyymmdd,
            news_entries,
            routed,
            source);
    }
    append_logf(
        "KBO military selection reached source=%s event=%s year=%u routed=%d seeded=%d returned=%d refreshed=%d news=%d queued_left=%d",
        source != NULL ? source : "",
        event_name != NULL ? event_name : "",
        event_year,
        routed,
        seeded,
        returned,
        refreshed,
        news_created,
        kbo_count_military_draft_candidates_for_year((uint16_t)event_year));
    return 1;
}
