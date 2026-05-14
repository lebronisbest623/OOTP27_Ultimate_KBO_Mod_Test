#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"
#include "ai/independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../core/teams/core_team_collect.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/common/policy/foreign_player_policy.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../assignment/assignment/team_assignment.h"
#include "../assignment/org_query/team_org_assignment_query.h"
#include "../classification/team_classification.h"
#include "../control/team_human_control.h"
#include "../lookup/team_lookup.h"
#include "../names/team_name_cache.h"
#include "../names/team_string.h"
#include "independent_acquisition_window.h"

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

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    uintptr_t league_ptr = league_id != 0u ? kbo_find_league_ptr_from_id(league_id) : 0u;
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
                sizeof(uint8_t))) {
        return today_serial - open_serial <= (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days;
    }

    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (phase != 2u && phase != 3u) {
        static uint32_t last_logged_closed_date = 0u;
        if (last_logged_closed_date != today) {
            last_logged_closed_date = today;
            kbo_log_runtimef(
                "independent acquisition AI window closed source=league_phase today=%u open=%u league=%u phase=%u",
                today,
                open_date,
                league_id,
                (uint32_t)phase);
        }
        return 0;
    }

    return 1;
}

static int64_t kbo_independent_acquisition_seller_fit_score(
    const KboIndependentAcquisitionQueuedRequest* request,
    uint8_t* player,
    uint8_t* buyer_team,
    int32_t cash_cost);

static int kbo_independent_acquisition_team_name_placeholder(const char* text)
{
    return text == NULL
        || text[0] == '\0'
        || _stricmp(text, "Team") == 0
        || _stricmp(text, "Unknown") == 0;
}

static void kbo_independent_acquisition_copy_team_name(
    uint8_t* team,
    uint32_t team_id,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));

        if (!kbo_independent_acquisition_team_name_placeholder(full_name) && strchr(full_name, ' ') != NULL) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(city)
                && !kbo_independent_acquisition_team_name_placeholder(nickname)
                && _stricmp(city, nickname) != 0) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(full_name)) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(nickname)) {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(city)) {
            snprintf(out, out_size, "%s", city);
            return;
        }
    }

    snprintf(out, out_size, "Team #%u", team_id);
}

static int kbo_emit_independent_acquisition_transfer_news(
    uint32_t today,
    uint8_t* player,
    uint8_t* buyer_team,
    uint8_t* seller_team,
    uint32_t player_id,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    int32_t cash_cost,
    const char* source)
{
    if (today == 0u || player == NULL || buyer_team == NULL || player_id == 0u || buyer_team_id == 0u) {
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (league_id == 0u) {
        league_id = kbo_get_foreign_waiver_league_id();
    }
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char player_name[96] = {0};
    kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", player_id);
    }

    char buyer_name[96] = {0};
    char seller_name[96] = {0};
    kbo_independent_acquisition_copy_team_name(buyer_team, buyer_team_id, buyer_name, sizeof(buyer_name));
    kbo_independent_acquisition_copy_team_name(seller_team, seller_team_id, seller_name, sizeof(seller_name));

    char player_link[128] = {0};
    char buyer_link[128] = {0};
    char seller_link[128] = {0};
    char cash_cost_text[32] = {0};
    snprintf(player_link, sizeof(player_link), "<%s:player#%u>", player_name, player_id);
    snprintf(buyer_link, sizeof(buyer_link), "<%s:team#%u>", buyer_name, buyer_team_id);
    if (seller_team_id != 0u) {
        snprintf(seller_link, sizeof(seller_link), "<%s:team#%u>", seller_name, seller_team_id);
    } else {
        snprintf(seller_link, sizeof(seller_link), "%s", seller_name);
    }
    snprintf(cash_cost_text, sizeof(cash_cost_text), "%d", cash_cost);

    char title[180] = {0};
    char body[1024] = {0};
    const KboNewsTemplateVar vars[] = {
        {"player_name", player_name},
        {"player_link", player_link},
        {"buyer_team_name", buyer_name},
        {"buyer_team_link", buyer_link},
        {"seller_team_name", seller_name},
        {"seller_team_link", seller_link},
        {"cash_cost", cash_cost_text},
    };
    if (!kbo_news_template_render_key(
            "custom_event.independent_team_acquisition.transfer.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "custom_event.independent_team_acquisition.transfer.news.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "independent acquisition transfer news skipped missing_template source=%s player=%u buyer=%u seller=%u",
            source != NULL ? source : "",
            player_id,
            buyer_team_id,
            seller_team_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_log_runtimef(
        "independent acquisition transfer news source=%s player=%u buyer=%u seller=%u league=%u cash_cost=%d created=%d",
        source != NULL ? source : "",
        player_id,
        buyer_team_id,
        seller_team_id,
        league_id,
        cash_cost,
        created);
    return created;
}

static int kbo_run_independent_team_acquisition_seller_ai(
    uint32_t today,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const char* source)
{
    KboIndependentAcquisitionQueuedRequest queue[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    int request_count = kbo_independent_acquisition_load_requests(
        today / 10000u,
        queue,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);
    if (request_count <= 0) {
        return 0;
    }

    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int decided = 0;
    int transferred = 0;
    int limit_blocked = 0;
    for (int i = 0; i < request_count; i++) {
        KboIndependentAcquisitionQueuedRequest* group = &queue[i];
        if (group->player_id == 0u) {
            continue;
        }
        uintptr_t player_ptr = kbo_independent_acquisition_find_player_snapshot(
            player_snapshot,
            player_count,
            group->player_id);
        uint8_t* player = (uint8_t*)player_ptr;
        KboIndependentAcquisitionQueuedRequest* best = group;
        int64_t best_fit_score = INT64_MIN;
        for (int j = i; j < request_count; j++) {
            if (queue[j].player_id != group->player_id
                    || queue[j].seller_team_id != group->seller_team_id) {
                continue;
            }
            uint8_t* candidate_team = find_kbo_team_by_numeric_id_any_league(queue[j].buyer_team_id, 1);
            int32_t candidate_cash_cost = queue[j].cash_cost;
            if (candidate_cash_cost <= 0 && player != NULL) {
                candidate_cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
                queue[j].cash_cost = candidate_cash_cost;
            }
            int64_t fit_score = kbo_independent_acquisition_seller_fit_score(
                &queue[j],
                player,
                candidate_team,
                candidate_cash_cost);
            if (fit_score > best_fit_score
                    || (fit_score == best_fit_score && queue[j].request_score > best->request_score)) {
                best = &queue[j];
                best_fit_score = fit_score;
            }
        }
        for (int j = i + 1; j < request_count; j++) {
            if (queue[j].player_id == group->player_id
                    && queue[j].seller_team_id == group->seller_team_id) {
                queue[j].player_id = 0u;
            }
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(best->buyer_team_id, 1);
        uint8_t* seller_team = find_kbo_team_by_numeric_id_any_league(best->seller_team_id, 1);
        int moved = 0;
        int cash_charged = 0;
        int32_t old_cash = 0;
        int32_t new_cash = 0;
        int32_t cash_cost = best->cash_cost;
        if (cash_cost <= 0 && player != NULL) {
            cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
            best->cash_cost = cash_cost;
        }
        int seller_transfers = kbo_independent_acquisition_transferred_count(
            best->season,
            best->seller_team_id);
        int seller_limit_reached = seller_transfers >= seller_transfer_limit;
        if (seller_limit_reached) {
            limit_blocked++;
        }
        if (!seller_limit_reached
                && player != NULL
                && buyer_team != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                && kbo_independent_acquisition_player_status_ok(player)
                && kbo_player_current_assignment_matches_team_or_affiliate(player, best->seller_team_id)
                && !kbo_player_current_assignment_matches_team_or_affiliate(player, best->buyer_team_id)
                && kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
            int pre = 0;
            int reg = 0;
            int attach = 0;
            uint32_t buyer_league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_like_ootp(player, buyer_team, buyer_league_id, &pre, &reg, &attach);
            moved = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == best->buyer_team_id;
            if (moved) {
                cash_charged = kbo_independent_acquisition_charge_team_cash(
                        buyer_team,
                        cash_cost,
                        &old_cash,
                        &new_cash);
                if (!cash_charged) {
                    kbo_log_runtimef(
                        "independent acquisition seller AI cash charge failed source=%s buyer=%u player=%u cost=%d",
                        source != NULL ? source : "",
                        best->buyer_team_id,
                        best->player_id,
                        cash_cost);
                }
            }
        }
        if (moved && cash_charged) {
            kbo_emit_independent_acquisition_transfer_news(
                today,
                player,
                buyer_team,
                seller_team,
                best->player_id,
                best->buyer_team_id,
                best->seller_team_id,
                cash_cost,
                source);
        }

        if (kbo_independent_acquisition_append_decision(today, best, moved, old_cash, new_cash, source)) {
            decided++;
            if (moved) {
                transferred++;
            }
            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)best->request_score);
            kbo_log_runtimef(
                "independent acquisition seller AI decision source=%s seller=%u player=%u buyer=%u score=%s cash_cost=%d old_cash=%d new_cash=%d transferred=%d seller_transfers=%d seller_transfer_limit=%d",
                source != NULL ? source : "",
                best->seller_team_id,
                best->player_id,
                best->buyer_team_id,
                request_score_text,
                cash_cost,
                old_cash,
                new_cash,
                moved,
                seller_transfers,
                seller_transfer_limit);
        }
    }

    kbo_log_runtimef(
        "independent acquisition seller AI summary source=%s today=%u queued=%d decided=%d transferred=%d limit_blocked=%d seller_transfer_limit=%d",
        source != NULL ? source : "",
        today,
        request_count,
        decided,
        transferred,
        limit_blocked,
        seller_transfer_limit);
    return transferred;
}

static int64_t kbo_independent_acquisition_seller_fit_score(
    const KboIndependentAcquisitionQueuedRequest* request,
    uint8_t* player,
    uint8_t* buyer_team,
    int32_t cash_cost)
{
    if (request == NULL
            || player == NULL
            || buyer_team == NULL
            || cash_cost <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
        return INT64_MIN;
    }

    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u || buyer.team_id != request->buyer_team_id) {
        return INT64_MIN;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            buyer.team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            return INT64_MIN;
        }
    }

    return kbo_independent_acquisition_score_candidate_for_buyer(
        &buyer,
        player,
        effective_before,
        effective_limit);
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

    uint32_t season = today / 10000u;
    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    KboIndependentFuturesTeamLeague available_sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(available_sellers, 0, sizeof(available_sellers));
    int available_seller_count = 0;
    int capped_sellers = 0;
    for (int i = 0; i < seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(season, sellers[i].team_id);
        if (transfers >= seller_transfer_limit) {
            capped_sellers++;
            continue;
        }
        if (available_seller_count < KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS) {
            available_sellers[available_seller_count++] = sellers[i];
        }
    }
    if (capped_sellers > 0) {
        kbo_log_runtimef(
            "independent acquisition AI seller transfer limit source=%s today=%u limit=%d sellers=%d available=%d capped=%d",
            source != NULL ? source : "",
            today,
            seller_transfer_limit,
            seller_count,
            available_seller_count,
            capped_sellers);
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
        if (available_seller_count <= 0
                || !kbo_independent_acquisition_choose_candidate_for_buyer(
                snapshot,
                player_count,
                available_sellers,
                available_seller_count,
                &buyer,
                &candidate)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller = NULL;
        for (int s = 0; s < available_seller_count; s++) {
            if (available_sellers[s].team_id == candidate.seller_team_id) {
                seller = &available_sellers[s];
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
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)candidate.request_score);
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
                "independent acquisition AI request source=%s action=%s buyer=%u seller=%u seller_csv=%s player=%u score=%s value=%d cash_cost=%d cash_available=%d effective=%u->%u limit=%u slot=%s",
                source != NULL ? source : "",
                existing_request ? "refresh_pending" : "new",
                buyer.team_id,
                candidate.seller_team_id,
                seller->team_csv_id,
                candidate.player_id,
                request_score_text,
                candidate.value_score,
                kbo_independent_acquisition_cash_cost_for_player((uint8_t*)candidate.player_ptr),
                buyer.cash_available,
                candidate.effective_before,
                candidate.effective_after,
                candidate.effective_limit,
                candidate.slot_type != 0u ? kbo_foreign_injury_slot_label(candidate.slot_type) : "none");
        }
    }

    kbo_log_runtimef(
        "independent acquisition AI summary source=%s today=%u requested=%d refreshed_pending=%d buyers=%d skipped_human=%d sellers=%d available_sellers=%d capped_sellers=%d seller_transfer_limit=%d player_count=%d team_scanned=%d team_unreadable=%d",
        source != NULL ? source : "",
        today,
        requested,
        refreshed_pending,
        considered_buyers,
        skipped_human,
        seller_count,
        available_seller_count,
        capped_sellers,
        seller_transfer_limit,
        player_count,
        scanned,
        unreadable);
    int transferred = kbo_run_independent_team_acquisition_seller_ai(
        today,
        snapshot,
        player_count,
        source);
    HeapFree(GetProcessHeap(), 0, snapshot);
    return requested + transferred;
}
