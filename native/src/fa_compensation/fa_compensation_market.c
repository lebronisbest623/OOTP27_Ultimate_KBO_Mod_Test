#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../bootstrap/ootp_offsets.h"
#include "../fa_market_classification/fa_market_classification.h"
#include "../fa_salary_snapshot/fa_salary_snapshot_parts/salary_snapshot_grade_rows.h"
#include "../foreign/foreign_waiver_player_eval.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_name_cache.h"
#include "fa_compensation_market.h"
#include "fa_compensation_records.h"
#include "fa_compensation_state.h"

static uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

static uint32_t kbo_fa_compensation_team_for_player(uint32_t fa_player_id, int signing_team)
{
    if (fa_player_id == 0u) {
        return 0u;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        return 0u;
    }

    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    uint32_t team_id = 0u;
    for (int i = 0; i < record_count; i++) {
        if (records[i].player_id == fa_player_id) {
            team_id = signing_team ? records[i].signing_team_id : records[i].original_team_id;
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, records);
    return team_id;
}

uint32_t kbo_fa_compensation_original_team_for_player(uint32_t fa_player_id)
{
    return kbo_fa_compensation_team_for_player(fa_player_id, 0);
}

uint32_t kbo_fa_compensation_signing_team_for_player(uint32_t fa_player_id)
{
    return kbo_fa_compensation_team_for_player(fa_player_id, 1);
}

int kbo_fa_compensation_build_market_row(
    uint8_t* player,
    KboFaMarketClassification* row,
    uint32_t league_id,
    uint32_t current_year,
    uint32_t today,
    const KboFaRules* rules)
{
    if (player == NULL || row == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    memset(row, 0, sizeof(*row));
    row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    row->current_team_id = 0u;
    row->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    row->original_team_id = kbo_get_player_original_team_id(player);
    row->current_league_id = league_id;
    row->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    row->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    row->retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
    row->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    row->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    row->fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    row->foreign_player = kbo_player_is_foreign_for_kbo_rights(player) ? 1u : 0u;
    row->draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
    row->draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
    row->draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
    row->draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
    row->generation_flags = player[OOTP27_PLAYER_GENERATION_FLAGS_OFFSET];
    row->generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    row->generation_grade = player[OOTP27_PLAYER_GENERATION_GRADE_OFFSET];
    row->generation_special = player[OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET];
    kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
    if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
        snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
    }

    KboFaMarketSeedCase* seeds = (KboFaMarketSeedCase*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_MARKET_SEED_MAX * sizeof(KboFaMarketSeedCase));
    KboFaRequalificationRecord* requal_records = (KboFaRequalificationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_REQUALIFICATION_MAX * sizeof(KboFaRequalificationRecord));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (seeds == NULL || requal_records == NULL || salary_grades == NULL) {
        if (seeds != NULL) { HeapFree(GetProcessHeap(), 0, seeds); }
        if (requal_records != NULL) { HeapFree(GetProcessHeap(), 0, requal_records); }
        if (salary_grades != NULL) { HeapFree(GetProcessHeap(), 0, salary_grades); }
        return 0;
    }

    char ignored_path[MAX_PATH] = {0};
    int seed_count = kbo_load_fa_market_seed_cases(seeds, KBO_FA_MARKET_SEED_MAX, ignored_path, sizeof(ignored_path));
    int requal_count = kbo_load_fa_requalification_records(requal_records, KBO_FA_REQUALIFICATION_MAX);
    int salary_grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        current_year,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        ignored_path,
        sizeof(ignored_path));

    KboFaMarketHistoryCase history;
    memset(&history, 0, sizeof(history));
    KboFaMarketClassification history_row = *row;
    kbo_load_fa_market_history_cases(&history_row, 1, &history, 1);
    kbo_fa_market_overlay_filing_history_cases(&history_row, 1, &history, 1);

    kbo_classify_fa_market_row(
        row,
        seeds,
        seed_count,
        requal_records,
        requal_count,
        &history,
        current_year,
        today);
    kbo_fa_market_apply_salary_snapshot_grade(row, salary_grades, salary_grade_count, rules);

    if (row->original_team_id == 0u && history_row.original_team_id != 0u) {
        row->original_team_id = history_row.original_team_id;
    }
    if (row->original_team_id == 0u) {
        row->original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    }
    HeapFree(GetProcessHeap(), 0, seeds);
    HeapFree(GetProcessHeap(), 0, requal_records);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    return 1;
}

