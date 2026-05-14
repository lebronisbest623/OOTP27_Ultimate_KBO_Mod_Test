#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ui.h"
#include "ai/independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/common/policy/foreign_player_policy.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../assignment/org_query/team_org_assignment_query.h"
#include "../classification/team_classification.h"
#include "../lookup/team_lookup.h"

typedef struct KboIndependentAcquisitionUiDecisionRow {
    uint32_t date;
    uint32_t season;
    uint32_t seller_team_id;
    uint32_t player_id;
    uint32_t buyer_team_id;
    int32_t value_score;
    int32_t cash_cost;
    int32_t old_cash;
    int32_t new_cash;
    int64_t request_score;
    uint8_t transferred;
} KboIndependentAcquisitionUiDecisionRow;

static void kbo_independent_acquisition_ui_copy_text(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }
    snprintf(out, out_size, "%s", text);
}

static int kbo_independent_acquisition_ui_window_open(
    uint32_t today,
    uint32_t open_date,
    uint32_t* out_close_date)
{
    if (out_close_date != NULL) {
        *out_close_date = 0u;
    }
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }

    uint32_t ttl = (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days;
    uint32_t close_date = kbo_add_days_yyyymmdd(open_date, ttl);
    if (out_close_date != NULL) {
        *out_close_date = close_date;
    }
    if (close_date == 0u) {
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
    return today <= close_date;
}

int kbo_independent_acquisition_ui_context(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiContext* out_context)
{
    if (out_context == NULL) {
        return 0;
    }
    memset(out_context, 0, sizeof(*out_context));
    out_context->buyer_team_id = buyer_team_id;
    out_context->policy_enabled = kbo_fix_enabled() && kbo_custom_foreign_policy_enabled();

    uint32_t today = 0u;
    if (kbo_get_current_yyyymmdd(&today)) {
        out_context->today = today;
        out_context->season = today / 10000u;
    }
    out_context->open_date = kbo_independent_team_acquisition_window_open_date();
    out_context->window_open = out_context->policy_enabled
        && kbo_independent_acquisition_ui_window_open(
            out_context->today,
            out_context->open_date,
            &out_context->close_date);

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    out_context->seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &out_context->seed_rows,
        &out_context->unresolved_seed_rows);

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team != NULL && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        KboIndependentAcquisitionBuyerState buyer;
        kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
        if (buyer.team_id != 0u) {
            out_context->buyer_valid = 1;
            out_context->buyer_active_count = buyer.active_count;
            out_context->buyer_effective_foreign_count = buyer.effective_foreign_count;
            out_context->buyer_cash = buyer.cash_available;
        }
    }
    return out_context->today != 0u && out_context->season != 0u;
}

static int64_t kbo_independent_acquisition_ui_score_candidate(
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
    int foreign = kbo_player_is_foreign_for_kbo_rights(player);
    int asian = foreign && kbo_player_is_asian_quota_candidate(player);

    if (buyer->active_count < 28u) {
        score += (int64_t)(28u - buyer->active_count) * 4000;
    }
    if (effective_before < effective_limit) {
        score += (int64_t)(effective_limit - effective_before) * 6000;
    }
    if (!foreign) {
        score += 3000;
    } else if (asian) {
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

static const KboIndependentFuturesTeamLeague* kbo_independent_acquisition_ui_seller_for_player(
    uint8_t* player,
    const KboIndependentFuturesTeamLeague* sellers,
    int seller_count)
{
    if (player == NULL || sellers == NULL || seller_count <= 0) {
        return NULL;
    }
    for (int i = 0; i < seller_count; i++) {
        if (sellers[i].team_id != 0u
                && kbo_player_current_assignment_matches_team_or_affiliate(player, sellers[i].team_id)) {
            return &sellers[i];
        }
    }
    return NULL;
}

static void kbo_independent_acquisition_ui_slot_label(
    uint8_t slot_type,
    int foreign_player,
    int asian_quota,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (!foreign_player) {
        snprintf(out, out_size, "DOM");
        return;
    }
    if (slot_type != 0u) {
        snprintf(out, out_size, "%s", kbo_foreign_injury_slot_label(slot_type));
        return;
    }
    snprintf(out, out_size, "%s", asian_quota ? "Asian" : "Foreign");
}

static void kbo_independent_acquisition_ui_insert_offer_row(
    KboIndependentAcquisitionUiOfferRow* rows,
    int* count,
    int max_rows,
    const KboIndependentAcquisitionUiOfferRow* row)
{
    if (rows == NULL || count == NULL || max_rows <= 0 || row == NULL) {
        return;
    }
    if (*count < max_rows) {
        rows[*count] = *row;
        (*count)++;
        return;
    }

    int min_index = 0;
    for (int i = 1; i < max_rows; i++) {
        if (rows[i].request_score < rows[min_index].request_score) {
            min_index = i;
        }
    }
    if (row->request_score > rows[min_index].request_score) {
        rows[min_index] = *row;
    }
}

static int kbo_independent_acquisition_ui_offer_row_cmp(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiOfferRow* left = (const KboIndependentAcquisitionUiOfferRow*)a;
    const KboIndependentAcquisitionUiOfferRow* right = (const KboIndependentAcquisitionUiOfferRow*)b;
    if (left->already_decided != right->already_decided) {
        return (int)left->already_decided - (int)right->already_decided;
    }
    if (left->already_requested != right->already_requested) {
        return (int)left->already_requested - (int)right->already_requested;
    }
    if (left->request_score < right->request_score) {
        return 1;
    }
    if (left->request_score > right->request_score) {
        return -1;
    }
    return 0;
}

int kbo_independent_acquisition_ui_collect_offer_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiOfferRow* out_rows,
    int max_rows,
    KboIndependentAcquisitionUiContext* out_context)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        if (out_context != NULL) { *out_context = context; }
        return 0;
    }
    if (out_context != NULL) {
        *out_context = context;
    }
    if (!context.window_open || !context.buyer_valid || context.seller_count <= 0) {
        return 0;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        NULL,
        NULL);
    if (seller_count <= 0) {
        return 0;
    }

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u) {
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_independent_acquisition_player_status_ok(player)
                || kbo_player_current_assignment_matches_team_or_affiliate(player, buyer.team_id)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller =
            kbo_independent_acquisition_ui_seller_for_player(player, sellers, seller_count);
        if (seller == NULL) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
        if (cash_cost <= 0 || buyer.cash_available < cash_cost) {
            continue;
        }

        uint32_t effective_before = buyer.effective_foreign_count;
        uint32_t effective_after = buyer.effective_foreign_count;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
        int asian_quota = foreign_player && kbo_player_is_asian_quota_candidate(player);
        if (foreign_player
                && !kbo_custom_foreign_policy_team_allows_candidate(
                    buyer.team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id)) {
            continue;
        }

        KboIndependentAcquisitionUiOfferRow row;
        memset(&row, 0, sizeof(row));
        row.player_ptr = player_ptr;
        row.player_id = player_id;
        row.seller_team_id = seller->team_id;
        row.seller_league_id = seller->league_id;
        row.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row.effective_before = effective_before;
        row.effective_after = effective_after;
        row.effective_limit = effective_limit;
        row.injured_player_id = injured_player_id;
        row.age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
            ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
            : 0u;
        row.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
        row.foreign_player = foreign_player ? 1u : 0u;
        row.asian_quota = asian_quota ? 1u : 0u;
        row.slot_type = slot_type;
        row.already_requested = kbo_independent_acquisition_request_exists(
            context.season,
            buyer.team_id,
            seller->team_id,
            player_id) ? 1u : 0u;
        row.already_decided = kbo_independent_acquisition_decision_exists(
            context.season,
            seller->team_id,
            player_id) ? 1u : 0u;
        row.value_score = kbo_foreign_waiver_value_score(player);
        row.cash_cost = cash_cost;
        row.request_score = kbo_independent_acquisition_ui_score_candidate(
            &buyer,
            player,
            effective_before,
            effective_limit);
        kbo_independent_acquisition_ui_slot_label(
            row.slot_type,
            row.foreign_player,
            row.asian_quota,
            row.slot_label,
            sizeof(row.slot_label));
        kbo_independent_acquisition_ui_insert_offer_row(out_rows, &count, max_rows, &row);
    }

    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_offer_row_cmp);
    }
    return count;
}

static int kbo_independent_acquisition_ui_data_path(
    const char* filename,
    char* out,
    size_t out_size)
{
    if (filename == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file(filename, out, out_size);
}

static char* kbo_independent_acquisition_ui_read_text_file(const char* filename, DWORD* out_read)
{
    if (out_read != NULL) {
        *out_read = 0u;
    }
    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_ui_data_path(filename, path, sizeof(path))) {
        return NULL;
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
        return NULL;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 8u * 1024u * 1024u) {
        CloseHandle(file);
        return NULL;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return NULL;
    }

    DWORD read = 0u;
    if (!ReadFile(file, buffer, size, &read, NULL) || read == 0u) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return NULL;
    }
    buffer[read] = '\0';
    CloseHandle(file);
    if (out_read != NULL) {
        *out_read = read;
    }
    return buffer;
}

static int kbo_independent_acquisition_ui_json_u32(
    const char* line,
    const char* key,
    uint32_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }
    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    unsigned long value = strtoul(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int kbo_independent_acquisition_ui_json_i32(
    const char* line,
    const char* key,
    int32_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }
    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    long value = strtol(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

static int kbo_independent_acquisition_ui_json_i64(
    const char* line,
    const char* key,
    int64_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }
    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    long long value = _strtoi64(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (int64_t)value;
    return 1;
}

static int kbo_independent_acquisition_ui_json_string(
    const char* line,
    const char* key,
    char* out,
    size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (line == NULL || key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":\"", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    size_t used = 0u;
    while (*p != '\0' && *p != '"' && used + 1u < out_size) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
        }
        out[used++] = *p++;
    }
    out[used] = '\0';
    return used > 0u;
}

static int kbo_independent_acquisition_ui_parse_request_line(
    const char* line,
    KboIndependentAcquisitionUiRequestRow* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    KboIndependentAcquisitionUiRequestRow row;
    memset(&row, 0, sizeof(row));
    if (!kbo_independent_acquisition_ui_json_u32(line, "date", &row.date)
            || !kbo_independent_acquisition_ui_json_u32(line, "season", &row.season)
            || !kbo_independent_acquisition_ui_json_u32(line, "buyer_team_id", &row.buyer_team_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "seller_team_id", &row.seller_team_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "player_id", &row.player_id)
            || !kbo_independent_acquisition_ui_json_i64(line, "request_score", &row.request_score)) {
        return 0;
    }

    uint32_t tmp = 0u;
    kbo_independent_acquisition_ui_json_u32(line, "nation_id", &row.nation_id);
    if (kbo_independent_acquisition_ui_json_u32(line, "pitcher", &tmp)) {
        row.pitcher = tmp ? 1u : 0u;
    }
    tmp = 0u;
    if (kbo_independent_acquisition_ui_json_u32(line, "asian_quota", &tmp)) {
        row.asian_quota = tmp ? 1u : 0u;
    }
    kbo_independent_acquisition_ui_json_i32(line, "cash_cost", &row.cash_cost);
    kbo_independent_acquisition_ui_json_i32(line, "value_score", &row.value_score);
    kbo_independent_acquisition_ui_json_u32(line, "effective_before", &row.effective_before);
    kbo_independent_acquisition_ui_json_u32(line, "effective_after", &row.effective_after);
    kbo_independent_acquisition_ui_json_u32(line, "effective_limit", &row.effective_limit);
    kbo_independent_acquisition_ui_json_u32(line, "injured_player_id", &row.injured_player_id);
    kbo_independent_acquisition_ui_json_string(line, "slot_type", row.slot_label, sizeof(row.slot_label));
    if (row.slot_label[0] == '\0') {
        kbo_independent_acquisition_ui_copy_text(row.slot_label, sizeof(row.slot_label), "-");
    }

    uint32_t current_team_id = 0u;
    uint32_t current_league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(row.player_id, &current_team_id, &current_league_id);
    (void)current_team_id;
    (void)current_league_id;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        row.player_ptr = (uintptr_t)player;
        if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
            row.age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        }
        if (row.nation_id == 0u) {
            row.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        }
    }

    *out = row;
    return row.season != 0u
        && row.buyer_team_id != 0u
        && row.seller_team_id != 0u
        && row.player_id != 0u;
}

static int kbo_independent_acquisition_ui_parse_decision_line(
    const char* line,
    KboIndependentAcquisitionUiDecisionRow* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    KboIndependentAcquisitionUiDecisionRow row;
    memset(&row, 0, sizeof(row));
    if (!kbo_independent_acquisition_ui_json_u32(line, "date", &row.date)
            || !kbo_independent_acquisition_ui_json_u32(line, "season", &row.season)
            || !kbo_independent_acquisition_ui_json_u32(line, "seller_team_id", &row.seller_team_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "player_id", &row.player_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "buyer_team_id", &row.buyer_team_id)
            || !kbo_independent_acquisition_ui_json_i64(line, "request_score", &row.request_score)) {
        return 0;
    }
    kbo_independent_acquisition_ui_json_i32(line, "value_score", &row.value_score);
    kbo_independent_acquisition_ui_json_i32(line, "cash_cost", &row.cash_cost);
    kbo_independent_acquisition_ui_json_i32(line, "old_cash", &row.old_cash);
    kbo_independent_acquisition_ui_json_i32(line, "new_cash", &row.new_cash);
    uint32_t transferred = 0u;
    if (kbo_independent_acquisition_ui_json_u32(line, "transferred", &transferred)) {
        row.transferred = transferred ? 1u : 0u;
    }
    *out = row;
    return row.season != 0u && row.seller_team_id != 0u && row.player_id != 0u;
}

static const KboIndependentAcquisitionUiDecisionRow*
kbo_independent_acquisition_ui_find_decision(
    const KboIndependentAcquisitionUiDecisionRow* decisions,
    int decision_count,
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (decisions == NULL || decision_count <= 0) {
        return NULL;
    }
    for (int i = 0; i < decision_count; i++) {
        const KboIndependentAcquisitionUiDecisionRow* row = &decisions[i];
        if (row->season == season
                && row->seller_team_id == seller_team_id
                && row->player_id == player_id) {
            return row;
        }
    }
    return NULL;
}

static int kbo_independent_acquisition_ui_load_decision_rows(
    uint32_t season,
    KboIndependentAcquisitionUiDecisionRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);
    DWORD read = 0u;
    char* buffer = kbo_independent_acquisition_ui_read_text_file(
        KBO_INDEPENDENT_ACQUISITION_DECISION_FILE,
        &read);
    if (buffer == NULL) {
        return 0;
    }

    int count = 0;
    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end && count < max_rows) {
        char* line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }
        char saved = *line_end;
        *line_end = '\0';
        KboIndependentAcquisitionUiDecisionRow row;
        if (kbo_independent_acquisition_ui_parse_decision_line(cursor, &row)
                && row.season == season) {
            out_rows[count++] = row;
        }
        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

static int kbo_independent_acquisition_ui_request_row_cmp_desc(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiRequestRow* left = (const KboIndependentAcquisitionUiRequestRow*)a;
    const KboIndependentAcquisitionUiRequestRow* right = (const KboIndependentAcquisitionUiRequestRow*)b;
    if (left->date < right->date) { return 1; }
    if (left->date > right->date) { return -1; }
    if (left->request_score < right->request_score) { return 1; }
    if (left->request_score > right->request_score) { return -1; }
    return 0;
}

static int kbo_independent_acquisition_ui_result_row_cmp_desc(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiResultRow* left = (const KboIndependentAcquisitionUiResultRow*)a;
    const KboIndependentAcquisitionUiResultRow* right = (const KboIndependentAcquisitionUiResultRow*)b;
    if (left->decision_date < right->decision_date) { return 1; }
    if (left->decision_date > right->decision_date) { return -1; }
    if (left->request.date < right->request.date) { return 1; }
    if (left->request.date > right->request.date) { return -1; }
    return 0;
}

int kbo_independent_acquisition_ui_load_pending_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiRequestRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        return 0;
    }

    DWORD read = 0u;
    char* buffer = kbo_independent_acquisition_ui_read_text_file(
        KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE,
        &read);
    if (buffer == NULL) {
        return 0;
    }

    int count = 0;
    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end && count < max_rows) {
        char* line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }
        char saved = *line_end;
        *line_end = '\0';
        KboIndependentAcquisitionUiRequestRow row;
        if (kbo_independent_acquisition_ui_parse_request_line(cursor, &row)
                && row.season == context.season
                && row.buyer_team_id == buyer_team_id
                && !kbo_independent_acquisition_decision_exists(row.season, row.seller_team_id, row.player_id)) {
            out_rows[count++] = row;
        }
        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_request_row_cmp_desc);
    }
    return count;
}

int kbo_independent_acquisition_ui_load_result_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiResultRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        return 0;
    }

    KboIndependentAcquisitionUiDecisionRow decisions[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    int decision_count = kbo_independent_acquisition_ui_load_decision_rows(
        context.season,
        decisions,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);
    if (decision_count <= 0) {
        return 0;
    }

    DWORD read = 0u;
    char* buffer = kbo_independent_acquisition_ui_read_text_file(
        KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE,
        &read);
    if (buffer == NULL) {
        return 0;
    }

    int count = 0;
    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end && count < max_rows) {
        char* line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }
        char saved = *line_end;
        *line_end = '\0';
        KboIndependentAcquisitionUiRequestRow request;
        if (kbo_independent_acquisition_ui_parse_request_line(cursor, &request)
                && request.season == context.season
                && request.buyer_team_id == buyer_team_id) {
            const KboIndependentAcquisitionUiDecisionRow* decision =
                kbo_independent_acquisition_ui_find_decision(
                    decisions,
                    decision_count,
                    request.season,
                    request.seller_team_id,
                    request.player_id);
            if (decision != NULL) {
                KboIndependentAcquisitionUiResultRow result;
                memset(&result, 0, sizeof(result));
                result.request = request;
                result.decision_date = decision->date;
                result.winning_buyer_team_id = decision->buyer_team_id;
                result.old_cash = decision->old_cash;
                result.new_cash = decision->new_cash;
                result.transferred = decision->transferred;
                out_rows[count++] = result;
            }
        }
        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_result_row_cmp_desc);
    }
    return count;
}

int kbo_independent_acquisition_ui_submit_offer(
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source)
{
    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)
            || !context.window_open
            || !context.buyer_valid
            || seller_team_id == 0u
            || player_id == 0u) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_CLOSED;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        NULL,
        NULL);
    const KboIndependentFuturesTeamLeague* seller = NULL;
    for (int i = 0; i < seller_count; i++) {
        if (sellers[i].team_id == seller_team_id) {
            seller = &sellers[i];
            break;
        }
    }
    if (seller == NULL) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    if (kbo_independent_acquisition_decision_exists(context.season, seller_team_id, player_id)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_DECIDED;
    }
    if (kbo_independent_acquisition_request_exists(
            context.season,
            buyer_team_id,
            seller_team_id,
            player_id)) {
        uint32_t current_team_id = 0u;
        uint32_t current_league_id = 0u;
        uint8_t* existing_player = kbo_find_player_by_id(player_id, &current_team_id, &current_league_id);
        (void)current_team_id;
        (void)current_league_id;
        if (existing_player != NULL) {
            kbo_record_custom_foreign_pending_offer(buyer_team_id, existing_player, context.today);
        }
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_DUPLICATE;
    }

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    uint32_t current_team_id = 0u;
    uint32_t current_league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(player_id, &current_team_id, &current_league_id);
    (void)current_team_id;
    (void)current_league_id;
    if (player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_independent_acquisition_player_status_ok(player)
            || !kbo_player_current_assignment_matches_team_or_affiliate(player, seller_team_id)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, buyer_team_id)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
    if (cash_cost <= 0 || buyer.cash_available < cash_cost) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_NO_CASH;
    }

    KboIndependentAcquisitionCandidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.player_ptr = (uintptr_t)player;
    candidate.player_id = player_id;
    candidate.seller_team_id = seller_team_id;
    candidate.seller_league_id = seller->league_id;
    candidate.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    candidate.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
    candidate.asian_quota = kbo_player_is_asian_quota_candidate(player) ? 1u : 0u;
    candidate.value_score = kbo_foreign_waiver_value_score(player);
    candidate.effective_before = buyer.effective_foreign_count;
    candidate.effective_after = buyer.effective_foreign_count;
    candidate.effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        if (!kbo_custom_foreign_policy_team_allows_candidate(
                buyer.team_id,
                player,
                &candidate.effective_before,
                &candidate.effective_after,
                &candidate.effective_limit,
                &candidate.slot_type,
                &candidate.injured_player_id)) {
            return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_BLOCKED;
        }
    }
    candidate.request_score = kbo_independent_acquisition_ui_score_candidate(
        &buyer,
        player,
        candidate.effective_before,
        candidate.effective_limit);

    if (!kbo_independent_acquisition_append_request(
            context.today,
            &candidate,
            &buyer,
            seller,
            source != NULL ? source : "hub_independent_offer")) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_FAILED;
    }
    kbo_record_custom_foreign_pending_offer(buyer.team_id, player, context.today);
    kbo_log_runtimef(
        "independent acquisition UI offer source=%s buyer=%u seller=%u player=%u score=%lld cash_cost=%d",
        source != NULL ? source : "",
        buyer.team_id,
        seller_team_id,
        player_id,
        (long long)candidate.request_score,
        cash_cost);
    return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_OK;
}
