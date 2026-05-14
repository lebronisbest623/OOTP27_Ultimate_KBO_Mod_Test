#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ui.h"
#include "../../ai/independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../foreign/injury/api/foreign_injury_labels.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../assignment/org_query/team_org_assignment_query.h"
#include "../../../lookup/team_lookup.h"

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
    if (!context.policy_enabled || !context.buyer_valid || context.seller_count <= 0) {
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
    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int available_seller_count = 0;
    for (int i = 0; i < seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(context.season, sellers[i].team_id);
        if (transfers >= seller_transfer_limit) {
            continue;
        }
        if (available_seller_count != i) {
            sellers[available_seller_count] = sellers[i];
        }
        available_seller_count++;
    }
    seller_count = available_seller_count;
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
        row.request_score = kbo_independent_acquisition_score_candidate_for_buyer(
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
