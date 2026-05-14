#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/teams/core_team_collect.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/policy/foreign_player_policy.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../assignment/org_query/team_org_assignment_query.h"
#include "../classification/team_classification.h"
#include "../control/team_human_control.h"
#include "../lookup/team_lookup.h"
#include "independent_acquisition_window.h"

#define KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE "independent_acquisition_requests.jsonl"
#define KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS 32
#define KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS 8

typedef struct KboIndependentAcquisitionBuyerState {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t active_count;
    uint32_t asian_hitters;
    uint32_t asian_pitchers;
    uint32_t non_asian_hitters;
    uint32_t non_asian_pitchers;
    uint32_t effective_foreign_count;
} KboIndependentAcquisitionBuyerState;

typedef struct KboIndependentAcquisitionCandidate {
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t seller_team_id;
    uint32_t seller_league_id;
    uint32_t nation_id;
    uint8_t pitcher;
    uint8_t asian_quota;
    int32_t value_score;
    int64_t request_score;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint8_t slot_type;
    uint32_t injured_player_id;
} KboIndependentAcquisitionCandidate;

static int kbo_independent_acquisition_request_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE,
        out,
        out_size);
}

static int kbo_independent_acquisition_window_active(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }

    uint32_t open_serial = kbo_date_serial(
        open_date / 10000u,
        (open_date / 100u) % 100u,
        open_date % 100u);
    uint32_t today_serial = kbo_date_serial(
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u);
    if (open_serial == 0u || today_serial == 0u || today_serial < open_serial) {
        return 0;
    }

    return today_serial - open_serial <= (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days;
}

static uint32_t kbo_independent_acquisition_active_count(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t count = 0u;
    uint32_t* active_ids = (uint32_t*)(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0u; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] != 0u) {
            count++;
        }
    }
    return count;
}

static void kbo_independent_acquisition_read_buyer_state(
    uint8_t* team,
    KboIndependentAcquisitionBuyerState* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    out->team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    out->league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    out->active_count = kbo_independent_acquisition_active_count(team);
    kbo_count_active_foreign_for_asian_quota(
        (uintptr_t)team,
        &out->asian_hitters,
        &out->asian_pitchers,
        &out->non_asian_hitters,
        &out->non_asian_pitchers);
    out->effective_foreign_count = kbo_effective_foreign_count_with_asian_quota(
        out->asian_hitters + out->asian_pitchers,
        out->non_asian_hitters + out->non_asian_pitchers);
}

static int kbo_independent_acquisition_player_status_ok(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint8_t*)(player + OOTP27_PLAYER_RETIRED_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_DFA_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_INJURY_ACTIVE_OFFSET) == 0u;
}

static int64_t kbo_independent_acquisition_score_candidate(
    const KboIndependentAcquisitionBuyerState* buyer,
    uint8_t* player,
    uint32_t effective_before,
    uint32_t effective_limit)
{
    if (buyer == NULL || player == NULL) {
        return INT64_MIN;
    }

    int64_t score = (int64_t)kbo_foreign_waiver_value_score(player);
    int pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u;
    int asian = kbo_player_is_asian_quota_candidate(player);

    if (buyer->active_count < 28u) {
        score += (int64_t)(28u - buyer->active_count) * 4000;
    }
    if (effective_before < effective_limit) {
        score += (int64_t)(effective_limit - effective_before) * 6000;
    }
    if (asian) {
        if (buyer->asian_hitters + buyer->asian_pitchers == 0u) {
            score += 9000;
        }
        if (pitcher && buyer->asian_pitchers == 0u) {
            score += 4000;
        } else if (!pitcher && buyer->asian_hitters == 0u) {
            score += 4000;
        }
    } else if (pitcher) {
        if (buyer->non_asian_pitchers == 0u) {
            score += 9000;
        } else if (buyer->non_asian_pitchers >= 2u) {
            score -= 5000;
        }
    } else {
        if (buyer->non_asian_hitters == 0u) {
            score += 9000;
        } else if (buyer->non_asian_hitters >= 2u) {
            score -= 5000;
        }
    }

    score += (int64_t)kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) * 120;
    score += (int64_t)kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) * 80;
    return score;
}

static void kbo_independent_acquisition_json_append_escaped(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (text == NULL) {
        text = "";
    }

    size_t used = strlen(out);
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0' && used + 2u < out_size; p++) {
        unsigned char ch = *p;
        if (ch == '"' || ch == '\\') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = (char)ch;
        } else if (ch == '\r') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'r';
        } else if (ch == '\n') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'n';
        } else if (ch < 0x20u) {
            if (used + 7u >= out_size) { break; }
            int len = snprintf(out + used, out_size - used, "\\u%04x", (unsigned int)ch);
            if (len <= 0 || used + (size_t)len >= out_size) { break; }
            used += (size_t)len;
        } else {
            out[used++] = (char)ch;
        }
    }
    out[used] = '\0';
}

static int kbo_independent_acquisition_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_request_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int exists = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char season_token[40] = {0};
        char buyer_token[48] = {0};
        char seller_token[48] = {0};
        char player_token[48] = {0};
        snprintf(season_token, sizeof(season_token), "\"season\":%u", season);
        snprintf(buyer_token, sizeof(buyer_token), "\"buyer_team_id\":%u", buyer_team_id);
        snprintf(seller_token, sizeof(seller_token), "\"seller_team_id\":%u", seller_team_id);
        snprintf(player_token, sizeof(player_token), "\"player_id\":%u", player_id);
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';
            if (strstr(cursor, season_token) != NULL
                    && strstr(cursor, buyer_token) != NULL
                    && strstr(cursor, seller_token) != NULL
                    && strstr(cursor, player_token) != NULL) {
                exists = 1;
                *line_end = saved;
                break;
            }
            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return exists;
}

static int kbo_independent_acquisition_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    const char* source)
{
    if (today == 0u || candidate == NULL || buyer == NULL || seller == NULL) {
        return 0;
    }
    if (kbo_independent_acquisition_request_exists(
            today / 10000u,
            buyer->team_id,
            candidate->seller_team_id,
            candidate->player_id)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_request_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "independent acquisition AI request skipped source=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            (unsigned long)GetLastError(),
            path);
        return 0;
    }

    char request_score_text[32] = {0};
    snprintf(
        request_score_text,
        sizeof(request_score_text),
        "%" PRId64,
        (int64_t)candidate->request_score);

    char line[1536] = {0};
    snprintf(
        line,
        sizeof(line),
        "{\"date\":%u,\"season\":%u,\"buyer_team_id\":%u,\"seller_team_id\":%u,\"seller_csv_id\":\"",
        today,
        today / 10000u,
        buyer->team_id,
        candidate->seller_team_id);
    kbo_independent_acquisition_json_append_escaped(line, sizeof(line), seller->team_csv_id);
    size_t used = strlen(line);
    snprintf(
        line + used,
        sizeof(line) - used,
        "\",\"player_id\":%u,\"nation_id\":%u,\"pitcher\":%u,\"asian_quota\":%u,\"value_score\":%d,\"request_score\":%s,\"effective_before\":%u,\"effective_after\":%u,\"effective_limit\":%u,\"slot_type\":\"%s\",\"injured_player_id\":%u,\"buyer_active_count\":%u,\"buyer_foreign_effective\":%u,\"source\":\"",
        candidate->player_id,
        candidate->nation_id,
        (uint32_t)candidate->pitcher,
        (uint32_t)candidate->asian_quota,
        candidate->value_score,
        request_score_text,
        candidate->effective_before,
        candidate->effective_after,
        candidate->effective_limit,
        candidate->slot_type != 0u ? kbo_foreign_injury_slot_label(candidate->slot_type) : "none",
        candidate->injured_player_id,
        buyer->active_count,
        buyer->effective_foreign_count);
    kbo_independent_acquisition_json_append_escaped(line, sizeof(line), source != NULL ? source : "");
    used = strlen(line);
    snprintf(line + used, sizeof(line) - used, "\"}\r\n");

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(line);
    int ok = len > 0u && WriteFile(file, line, len, &written, NULL) && written == len;
    CloseHandle(file);
    return ok;
}

static int kbo_independent_acquisition_choose_candidate_for_buyer(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const KboIndependentFuturesTeamLeague* sellers,
    int seller_count,
    const KboIndependentAcquisitionBuyerState* buyer,
    KboIndependentAcquisitionCandidate* out_candidate)
{
    if (out_candidate != NULL) {
        memset(out_candidate, 0, sizeof(*out_candidate));
        out_candidate->request_score = INT64_MIN;
    }
    if (player_snapshot == NULL
            || player_count <= 0
            || sellers == NULL
            || seller_count <= 0
            || buyer == NULL
            || buyer->team_id == 0u
            || out_candidate == NULL) {
        return 0;
    }

    KboIndependentAcquisitionCandidate best = {0};
    best.request_score = INT64_MIN;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_independent_acquisition_player_status_ok(player)
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || kbo_player_current_assignment_matches_team_or_affiliate(player, buyer->team_id)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller = NULL;
        for (int s = 0; s < seller_count; s++) {
            if (sellers[s].team_id != 0u
                    && kbo_player_current_assignment_matches_team_or_affiliate(player, sellers[s].team_id)) {
                seller = &sellers[s];
                break;
            }
        }
        if (seller == NULL) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }

        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            buyer->team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            continue;
        }

        int64_t request_score = kbo_independent_acquisition_score_candidate(
            buyer,
            player,
            effective_before,
            effective_limit);
        if (request_score <= best.request_score) {
            continue;
        }

        best.player_ptr = player_ptr;
        best.player_id = player_id;
        best.seller_team_id = seller->team_id;
        best.seller_league_id = seller->league_id;
        best.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        best.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
        best.asian_quota = kbo_player_is_asian_quota_candidate(player) ? 1u : 0u;
        best.value_score = kbo_foreign_waiver_value_score(player);
        best.request_score = request_score;
        best.effective_before = effective_before;
        best.effective_after = effective_after;
        best.effective_limit = effective_limit;
        best.slot_type = slot_type;
        best.injured_player_id = injured_player_id;
    }

    if (best.player_id == 0u || best.request_score == INT64_MIN) {
        return 0;
    }
    *out_candidate = best;
    return 1;
}

int kbo_run_independent_team_acquisition_ai(const char* source)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || read_kbo_localappdata_flag_file("disable_independent_acquisition_ai.txt")) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)
            || !kbo_independent_acquisition_window_active(today)) {
        return 0;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seed_rows = 0;
    int unresolved_rows = 0;
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &seed_rows,
        &unresolved_rows);
    if (seller_count <= 0) {
        kbo_log_runtimef(
            "independent acquisition AI skipped source=%s reason=no_resolved_seller today=%u seed_rows=%d unresolved=%d",
            source != NULL ? source : "",
            today,
            seed_rows,
            unresolved_rows);
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > 200000) {
        return 0;
    }
    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        return 0;
    }
    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        return 0;
    }
    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t buyer_team_ids[KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS] = {0};
    int scanned = 0;
    int unreadable = 0;
    int buyer_count = collect_kbo_league_team_ids(
        kbo_league_id,
        buyer_team_ids,
        KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS,
        &scanned,
        &unreadable);

    int requested = 0;
    int refreshed_pending = 0;
    int considered_buyers = 0;
    int skipped_human = 0;
    for (int i = 0; i < buyer_count; i++) {
        uint32_t buyer_team_id = buyer_team_ids[i];
        if (kbo_team_is_human_controlled(buyer_team_id, "independent_acquisition_ai")) {
            skipped_human++;
            continue;
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
        if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        KboIndependentAcquisitionBuyerState buyer;
        kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
        if (buyer.team_id == 0u) {
            continue;
        }
        considered_buyers++;

        KboIndependentAcquisitionCandidate candidate;
        if (!kbo_independent_acquisition_choose_candidate_for_buyer(
                snapshot,
                player_count,
                sellers,
                seller_count,
                &buyer,
                &candidate)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller = NULL;
        for (int s = 0; s < seller_count; s++) {
            if (sellers[s].team_id == candidate.seller_team_id) {
                seller = &sellers[s];
                break;
            }
        }
        if (seller == NULL) {
            continue;
        }

        int existing_request = kbo_independent_acquisition_request_exists(
            today / 10000u,
            buyer.team_id,
            candidate.seller_team_id,
            candidate.player_id);
        int request_available = existing_request
            || kbo_independent_acquisition_append_request(today, &candidate, &buyer, seller, source);
        if (request_available) {
            char request_score_text[32] = {0};
            snprintf(
                request_score_text,
                sizeof(request_score_text),
                "%" PRId64,
                (int64_t)candidate.request_score);
            kbo_record_custom_foreign_pending_offer(
                buyer.team_id,
                (uint8_t*)candidate.player_ptr,
                today);
            if (existing_request) {
                refreshed_pending++;
            } else {
                requested++;
            }
            kbo_log_runtimef(
                "independent acquisition AI request source=%s action=%s buyer=%u seller=%u seller_csv=%s player=%u score=%s value=%d effective=%u->%u limit=%u slot=%s",
                source != NULL ? source : "",
                existing_request ? "refresh_pending" : "new",
                buyer.team_id,
                candidate.seller_team_id,
                seller->team_csv_id,
                candidate.player_id,
                request_score_text,
                candidate.value_score,
                candidate.effective_before,
                candidate.effective_after,
                candidate.effective_limit,
                candidate.slot_type != 0u ? kbo_foreign_injury_slot_label(candidate.slot_type) : "none");
        }
    }

    kbo_log_runtimef(
        "independent acquisition AI summary source=%s today=%u requested=%d refreshed_pending=%d buyers=%d skipped_human=%d sellers=%d player_count=%d team_scanned=%d team_unreadable=%d",
        source != NULL ? source : "",
        today,
        requested,
        refreshed_pending,
        considered_buyers,
        skipped_human,
        seller_count,
        player_count,
        scanned,
        unreadable);
    HeapFree(GetProcessHeap(), 0, snapshot);
    return requested;
}
