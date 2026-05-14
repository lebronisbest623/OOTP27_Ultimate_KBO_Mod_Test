#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fa_declaration.h"
#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/core_league_context_parts/api/league_context_lookup.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../core/csv/core_csv.h"
#include "../core/logging/core_log.h"
#include "../core/news/live/core_live_news.h"
#include "../core/news/templates/core_news_templates.h"
#include "../fa_filing/fa_filing.h"
#include "../fa_filing/fa_filing_parts/fa_filing_csv_write_helpers.h"
#include "../fa_market_classification/api/fa_market_classification.h"
#include "../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/lookup/team_lookup.h"
#include "../team/names/team_name_cache.h"

#define KBO_FA_DECLARATION_MAX 4096
#define KBO_FA_DECLARATION_NEWS_DECLARED_LIMIT 8
#define KBO_FA_DECLARATION_NEWS_DEFERRED_LIMIT 6

typedef struct KboFaDeclarationCandidate {
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t declaration_date;
    uint32_t season;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t nation_id;
    uint16_t age;
    uint8_t contract_level;
    uint8_t dfa;
    uint8_t retired_flag;
    uint8_t declared;
    uint8_t from_market;
    int32_t salary;
    int32_t fa_demand;
    int32_t score;
    int32_t threshold;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
    int16_t career;
    char player_name[96];
    char case_label[48];
    char grade[12];
    char reason[192];
    char decision_reason[160];
} KboFaDeclarationCandidate;

static int kbo_get_fa_declaration_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_declarations.csv", out, out_size);
}

#include "fa_declaration_repair.inc"

int kbo_fa_declaration_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision)
{
    if (out_decision != NULL) {
        memset(out_decision, 0, sizeof(*out_decision));
    }
    if (player_id == 0u || out_decision == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_csv_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int found = 0;
    uint32_t best_date = 0u;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[13][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 13);
        if (field_count < 7 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboFaDeclarationDecision row;
        if (!kbo_fa_declaration_parse_decision_fields(&row, fields, field_count)) {
            continue;
        }
        if (row.player_id == player_id
                && (season == 0u || row.season == season)
                && row.declaration_date >= best_date) {
            found = 1;
            best_date = row.declaration_date;
            *out_decision = row;
        }
    }

    kbo_csv_reader_close(reader);
    return found;
}

#include "fa_declaration_news.inc"

#include "fa_declaration_decision.inc"

#include "fa_declaration_collect.inc"

#include "fa_declaration_csv.inc"

int kbo_handle_fa_declaration_event(uint32_t event_yyyymmdd, const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (event_yyyymmdd == 0u) {
        return -1;
    }

    uint32_t season = event_yyyymmdd / 10000u;
    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        league_id = OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    KboFaDeclarationCandidate* candidates = (KboFaDeclarationCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_DECLARATION_MAX * sizeof(KboFaDeclarationCandidate));
    KboFaMarketClassification* market_rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (candidates == NULL || market_rows == NULL || salary_grades == NULL) {
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        if (market_rows != NULL) { HeapFree(GetProcessHeap(), 0, market_rows); }
        if (salary_grades != NULL) { HeapFree(GetProcessHeap(), 0, salary_grades); }
        append_log_line("KBO FA declaration event skipped reason=allocation_failed");
        return -1;
    }

    char ignored_path[MAX_PATH] = {0};
    int grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        season,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        ignored_path,
        sizeof(ignored_path));

    KboFaMarketScanSummary market_summary;
    memset(&market_summary, 0, sizeof(market_summary));
    int market_count = kbo_collect_fa_market_classifications(
        league_id,
        market_rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &market_summary,
        0,
        "fa_declaration_event");

    int candidate_count = 0;
    int market_added = 0;
    for (int i = 0; i < market_count && candidate_count < KBO_FA_DECLARATION_MAX; i++) {
        market_added += kbo_fa_declaration_add_market_candidate(
            &market_rows[i],
            event_yyyymmdd,
            season,
            league_id,
            salary_grades,
            grade_count,
            candidates,
            &candidate_count);
    }

    int active_scanned = 0;
    int active_added = kbo_fa_declaration_collect_active_fallback(
        event_yyyymmdd,
        season,
        league_id,
        salary_grades,
        grade_count,
        candidates,
        &candidate_count,
        &active_scanned);
    if (active_added < 0 && market_summary.scanned == 0 && market_count == 0) {
        HeapFree(GetProcessHeap(), 0, candidates);
        HeapFree(GetProcessHeap(), 0, market_rows);
        HeapFree(GetProcessHeap(), 0, salary_grades);
        append_logf(
            "KBO FA declaration event deferred source=%s date=%u reason=no_player_vector",
            source != NULL ? source : "",
            event_yyyymmdd);
        return -1;
    }
    if (active_added < 0) {
        active_added = 0;
    }

    int declared = 0;
    int deferred = 0;
    int deferred_retry = 0;
    int deferred_no_market = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].declared) {
            declared++;
        } else {
            deferred++;
            if (strstr(candidates[i].decision_reason, "retry_after_down_year") != NULL) {
                deferred_retry++;
            } else if (strstr(candidates[i].decision_reason, "no_market_stay_original") != NULL) {
                deferred_no_market++;
            }
        }
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_fa_declaration_append_csv(
        candidates,
        candidate_count,
        source != NULL ? source : "fa_declaration_event",
        csv_path,
        sizeof(csv_path));
    if (!wrote) {
        HeapFree(GetProcessHeap(), 0, candidates);
        HeapFree(GetProcessHeap(), 0, market_rows);
        HeapFree(GetProcessHeap(), 0, salary_grades);
        return -1;
    }

    int retained_repaired = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].declared || candidates[i].player_ptr == 0u) {
            continue;
        }
        KboFaDeclarationDecision decision;
        memset(&decision, 0, sizeof(decision));
        decision.player_id = candidates[i].player_id;
        decision.declaration_date = candidates[i].declaration_date;
        decision.season = candidates[i].season;
        decision.declared = 0u;
        decision.team_id = candidates[i].team_id;
        decision.league_id = candidates[i].league_id;
        decision.contract_level = candidates[i].contract_level;
        decision.salary = candidates[i].salary;
        decision.fa_demand = candidates[i].fa_demand;
        decision.score = candidates[i].score;
        retained_repaired += kbo_fa_declaration_repair_retained_contract_salary(
            (uint8_t*)candidates[i].player_ptr,
            season,
            &decision,
            0,
            source != NULL ? source : "fa_declaration_event");
    }

    int detail_logs = 0;
    for (int i = 0; i < candidate_count && detail_logs < 40; i++) {
        if (!candidates[i].declared) {
            continue;
        }
        append_logf(
            "KBO FA declaration decided player=%u name=%s team=%u case=%s grade=%s score=%d threshold=%d reason=%s",
            candidates[i].player_id,
            candidates[i].player_name,
            candidates[i].team_id,
            candidates[i].case_label,
            candidates[i].grade,
            candidates[i].score,
            candidates[i].threshold,
            candidates[i].decision_reason);
        detail_logs++;
    }

    int deferred_logs = 0;
    for (int i = 0; i < candidate_count && deferred_logs < 40; i++) {
        if (candidates[i].declared) {
            continue;
        }
        append_logf(
            "KBO FA declaration deferred player=%u name=%s team=%u case=%s grade=%s score=%d threshold=%d reason=%s",
            candidates[i].player_id,
            candidates[i].player_name,
            candidates[i].team_id,
            candidates[i].case_label,
            candidates[i].grade,
            candidates[i].score,
            candidates[i].threshold,
            candidates[i].decision_reason);
        deferred_logs++;
    }

    append_logf(
        "KBO FA declaration event source=%s date=%u season=%u league=%u market_rows=%d market_candidates=%d active_scanned=%d active_candidates=%d candidates=%d declared=%d deferred=%d retry=%d no_market=%d grades=%d retained_repaired=%d csv=%s",
        source != NULL ? source : "",
        event_yyyymmdd,
        season,
        league_id,
        market_count,
        market_added,
        active_scanned,
        active_added,
        candidate_count,
        declared,
        deferred,
        deferred_retry,
        deferred_no_market,
        grade_count,
        retained_repaired,
        csv_path);

    kbo_emit_fa_declaration_summary_news(
        event_yyyymmdd,
        season,
        league_id,
        candidates,
        candidate_count,
        declared,
        deferred,
        deferred_retry,
        deferred_no_market,
        source != NULL ? source : "fa_declaration_event");

    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, market_rows);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    return 1;
}
