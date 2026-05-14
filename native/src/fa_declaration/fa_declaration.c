#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fa_declaration.h"
#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/core_league_context_parts/api/league_context_lookup.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../core/logging/core_log.h"
#include "../fa_filing/fa_filing.h"
#include "../fa_filing/fa_filing_parts/fa_filing_csv_write_helpers.h"
#include "../fa_market_classification/api/fa_market_classification.h"
#include "../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/lookup/team_lookup.h"
#include "../team/names/team_name_cache.h"

#define KBO_FA_DECLARATION_MAX 4096

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

static int kbo_fa_declaration_case_candidate(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_DEFERRED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0;
}

static int kbo_fa_declaration_find_candidate(
    const KboFaDeclarationCandidate* candidates,
    int count,
    uint32_t player_id)
{
    if (candidates == NULL || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].player_id == player_id) {
            return i;
        }
    }
    return -1;
}

static uint32_t kbo_fa_declaration_team_league(uint32_t team_id, uint32_t fallback_league_id)
{
    if (team_id == 0u) {
        return fallback_league_id;
    }
    uint32_t league_id = kbo_fa_filing_team_league_id(team_id);
    return league_id != 0u ? league_id : fallback_league_id;
}

static int32_t kbo_fa_declaration_contract_salary_for_season(
    uint8_t* player,
    uint32_t season,
    int32_t* out_next_salary)
{
    if (out_next_salary != NULL) {
        *out_next_salary = 0;
    }
    if (player == NULL
            || season == 0u
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(int32_t))
            || !memory_range_readable(
                player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
                OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        return 0;
    }

    int32_t years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS] = {0};
    for (uint32_t i = 0u; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }

    int32_t start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    uint32_t index = 0u;
    int have_index = 0;
    if (start_year > 0 && season >= (uint32_t)start_year) {
        uint32_t candidate_index = season - (uint32_t)start_year;
        if (candidate_index < OOTP27_PLAYER_CONTRACT_SALARY_YEARS) {
            index = candidate_index;
            have_index = 1;
        }
    }
    if (!have_index) {
        index = 0u;
    }

    int32_t current_salary = years[index];
    int32_t next_salary = 0;
    if (index + 1u < OOTP27_PLAYER_CONTRACT_SALARY_YEARS) {
        next_salary = years[index + 1u];
    }
    if (out_next_salary != NULL) {
        *out_next_salary = next_salary;
    }
    if (current_salary <= 0) {
        for (uint32_t i = 0u; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
            if (years[i] > 0) {
                current_salary = years[i];
                break;
            }
        }
    }
    return current_salary;
}

static void kbo_fa_declaration_fill_from_player(
    KboFaDeclarationCandidate* candidate,
    uint8_t* player,
    uint32_t season)
{
    if (candidate == NULL || player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    candidate->player_ptr = (uintptr_t)player;
    candidate->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    candidate->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    candidate->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    candidate->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    candidate->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    candidate->retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
    candidate->fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    candidate->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    candidate->talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    candidate->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    candidate->career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    candidate->score = kbo_foreign_waiver_value_score(player);
    if (candidate->salary <= 0) {
        candidate->salary = kbo_fa_declaration_contract_salary_for_season(player, season, NULL);
    }
    kbo_copy_player_display_name(player, candidate->player_name, sizeof(candidate->player_name));
    if (candidate->player_name[0] == '\0' || strcmp(candidate->player_name, "Unknown player") == 0) {
        snprintf(candidate->player_name, sizeof(candidate->player_name), "Player #%u", candidate->player_id);
    }
}

static void kbo_fa_declaration_apply_salary_grade(
    KboFaDeclarationCandidate* candidate,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count)
{
    if (candidate == NULL || candidate->player_id == 0u || grades == NULL || grade_count <= 0) {
        return;
    }
    const KboFaSalarySnapshotGrade* grade = kbo_find_fa_salary_snapshot_grade(
        grades,
        grade_count,
        candidate->player_id);
    if (grade == NULL) {
        return;
    }
    if (candidate->grade[0] == '\0' || strcmp(candidate->grade, "UNKNOWN") == 0) {
        snprintf(candidate->grade, sizeof(candidate->grade), "%s", grade->grade);
    }
    if (candidate->salary <= 0) {
        candidate->salary = grade->salary;
    }
    if (candidate->team_id == 0u) {
        candidate->team_id = grade->ranking_team_id;
    }
    if (candidate->player_name[0] == '\0' && grade->player_name[0] != '\0') {
        snprintf(candidate->player_name, sizeof(candidate->player_name), "%s", grade->player_name);
    }
}

static void kbo_fa_declaration_decide(KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return;
    }

    if (strcmp(candidate->case_label, "KBO_FA_APPROVED") == 0) {
        candidate->declared = 1u;
        candidate->threshold = 0;
        snprintf(candidate->decision_reason, sizeof(candidate->decision_reason), "manual approved FA seed");
        return;
    }
    if (strcmp(candidate->case_label, "KBO_FA_DEFERRED") == 0) {
        candidate->declared = 0u;
        candidate->threshold = 2147483647;
        snprintf(candidate->decision_reason, sizeof(candidate->decision_reason), "manual deferred FA seed");
        return;
    }
    if (strcmp(candidate->case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) {
        candidate->declared = 0u;
        candidate->threshold = 2147483647;
        snprintf(candidate->decision_reason, sizeof(candidate->decision_reason), "manual eligible-not-approved FA seed");
        return;
    }

    int32_t threshold = 68000;
    if (strcmp(candidate->grade, "A") == 0) {
        threshold = 50000;
    } else if (strcmp(candidate->grade, "B") == 0) {
        threshold = 56000;
    } else if (strcmp(candidate->grade, "C") == 0) {
        threshold = 64000;
    }

    if (candidate->age >= 37u) {
        threshold += 20000;
    } else if (candidate->age >= 34u) {
        threshold += 8000;
    } else if (candidate->age >= 29u && candidate->age <= 31u) {
        threshold -= 4000;
    }

    if (candidate->salary >= 700000000) {
        threshold += 8000;
    } else if (candidate->salary > 0 && candidate->salary <= 120000000) {
        threshold -= 3000;
    }

    if (strcmp(candidate->case_label, "KBO_FA_ELIGIBLE_PROXY") == 0) {
        threshold += 8000;
    }

    candidate->threshold = threshold;
    candidate->declared = candidate->score >= threshold ? 1u : 0u;
    snprintf(
        candidate->decision_reason,
        sizeof(candidate->decision_reason),
        "%s score=%d threshold=%d age=%u grade=%s salary=%d",
        candidate->declared ? "ai_declared" : "ai_deferred",
        candidate->score,
        candidate->threshold,
        (uint32_t)candidate->age,
        candidate->grade[0] != '\0' ? candidate->grade : "UNKNOWN",
        candidate->salary);
}

static int kbo_fa_declaration_add_market_candidate(
    const KboFaMarketClassification* row,
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    KboFaDeclarationCandidate* candidates,
    int* candidate_count)
{
    if (row == NULL
            || candidates == NULL
            || candidate_count == NULL
            || *candidate_count >= KBO_FA_DECLARATION_MAX
            || !kbo_fa_declaration_case_candidate(row->case_label)
            || row->foreign_player != 0u
            || row->retired_flag != 0u
            || row->player_id == 0u) {
        return 0;
    }
    if (kbo_fa_declaration_find_candidate(candidates, *candidate_count, row->player_id) >= 0) {
        return 0;
    }

    KboFaDeclarationCandidate* candidate = &candidates[(*candidate_count)++];
    memset(candidate, 0, sizeof(*candidate));
    candidate->player_id = row->player_id;
    candidate->declaration_date = event_yyyymmdd;
    candidate->season = season;
    candidate->team_id = row->original_team_id != 0u ? row->original_team_id : row->active_team_id;
    candidate->league_id = kbo_fa_declaration_team_league(candidate->team_id, league_id);
    candidate->nation_id = row->nation_id;
    candidate->age = row->age;
    candidate->contract_level = row->contract_level;
    candidate->dfa = row->dfa;
    candidate->retired_flag = row->retired_flag;
    candidate->salary = row->fa_grade_salary;
    candidate->fa_demand = row->fa_demand;
    candidate->from_market = 1u;
    snprintf(candidate->player_name, sizeof(candidate->player_name), "%s", row->player_name);
    snprintf(candidate->case_label, sizeof(candidate->case_label), "%s", row->case_label);
    snprintf(candidate->grade, sizeof(candidate->grade), "%s", row->grade[0] != '\0' ? row->grade : "UNKNOWN");
    snprintf(candidate->reason, sizeof(candidate->reason), "%s", row->reason);

    uint32_t ignored_team = 0u;
    uint32_t ignored_league = 0u;
    uint8_t* player = kbo_find_player_by_id(row->player_id, &ignored_team, &ignored_league);
    kbo_fa_declaration_fill_from_player(candidate, player, season);
    kbo_fa_declaration_apply_salary_grade(candidate, grades, grade_count);
    kbo_fa_declaration_decide(candidate);
    return 1;
}

static int kbo_fa_declaration_team_is_kbo(uint32_t team_id, uint32_t league_id)
{
    if (team_id == 0u) {
        return 0;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    return team_league_id != 0u && (league_id == 0u || team_league_id == league_id);
}

static int kbo_fa_declaration_active_player_candidate(
    uint8_t* player,
    uint32_t season,
    uint32_t league_id,
    int32_t* out_salary)
{
    if (out_salary != NULL) {
        *out_salary = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (player_id == 0u
            || current_team_id == 0u
            || nation_id != OOTP27_KBO_KOREA_NATION_ID
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u
            || age < 29u
            || !kbo_fa_declaration_team_is_kbo(current_team_id, league_id)) {
        return 0;
    }

    int32_t next_salary = 0;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, season, &next_salary);
    int32_t score = kbo_foreign_waiver_value_score(player);
    if (salary <= 0 || next_salary > 0) {
        return 0;
    }
    if (score < 45000 && salary < 120000000) {
        return 0;
    }

    if (out_salary != NULL) {
        *out_salary = salary;
    }
    return 1;
}

static int kbo_fa_declaration_collect_active_fallback(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    KboFaDeclarationCandidate* candidates,
    int* candidate_count,
    int* out_scanned)
{
    if (out_scanned != NULL) {
        *out_scanned = 0;
    }
    if (candidates == NULL || candidate_count == NULL) {
        return -1;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return -1;
    }

    int added = 0;
    for (int32_t i = 0; i < player_count && *candidate_count < KBO_FA_DECLARATION_MAX; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (out_scanned != NULL) {
            (*out_scanned)++;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (kbo_fa_declaration_find_candidate(candidates, *candidate_count, player_id) >= 0) {
            continue;
        }

        int32_t salary = 0;
        if (!kbo_fa_declaration_active_player_candidate(player, season, league_id, &salary)) {
            continue;
        }

        KboFaDeclarationCandidate* candidate = &candidates[(*candidate_count)++];
        memset(candidate, 0, sizeof(*candidate));
        candidate->declaration_date = event_yyyymmdd;
        candidate->season = season;
        candidate->team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        candidate->league_id = kbo_fa_declaration_team_league(candidate->team_id, league_id);
        candidate->salary = salary;
        snprintf(candidate->case_label, sizeof(candidate->case_label), "KBO_FA_ELIGIBLE_PROXY");
        snprintf(candidate->grade, sizeof(candidate->grade), "UNKNOWN");
        snprintf(
            candidate->reason,
            sizeof(candidate->reason),
            "active domestic expiring-contract proxy; actual FA transition not linked yet");
        kbo_fa_declaration_fill_from_player(candidate, player, season);
        kbo_fa_declaration_apply_salary_grade(candidate, grades, grade_count);
        kbo_fa_declaration_decide(candidate);
        added++;
    }
    return added;
}

static int kbo_fa_declaration_append_csv(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA declaration csv open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    DWORD high = 0;
    DWORD size = GetFileSize(file, &high);
    int needs_header = high == 0u && size == 0u;
    if (needs_header) {
        kbo_fa_filing_write_raw(
            file,
            "date,season,player_id,name,declared,team_id,league_id,nation_id,age,contract_level,"
            "salary,fa_demand,score,threshold,grade,kbo_case,overall,talent,ratings,career,source,reason,decision\r\n");
    }

    for (int i = 0; i < candidate_count; i++) {
        const KboFaDeclarationCandidate* c = &candidates[i];
        if (c->player_id == 0u) {
            continue;
        }
        char prefix[192] = {0};
        snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,",
            c->declaration_date,
            c->season,
            c->player_id);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, c->player_name);
        snprintf(
            prefix,
            sizeof(prefix),
            ",%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,",
            (uint32_t)c->declared,
            c->team_id,
            c->league_id,
            c->nation_id,
            (uint32_t)c->age,
            (uint32_t)c->contract_level,
            c->salary,
            c->fa_demand,
            c->score,
            c->threshold);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, c->grade);
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, c->case_label);
        snprintf(
            prefix,
            sizeof(prefix),
            ",%d,%d,%d,%d,",
            (int)c->overall,
            (int)c->talent,
            (int)c->ratings,
            (int)c->career);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, source != NULL ? source : "fa_declaration_event");
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, c->reason);
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, c->decision_reason);
        kbo_fa_filing_write_raw(file, "\r\n");
    }

    CloseHandle(file);
    return 1;
}

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
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].declared) {
            declared++;
        } else {
            deferred++;
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

    int detail_logs = 0;
    for (int i = 0; i < candidate_count && detail_logs < 40; i++) {
        if (!candidates[i].declared) {
            continue;
        }
        append_logf(
            "KBO FA declaration decided player=%u name=%s team=%u case=%s grade=%s score=%d threshold=%d",
            candidates[i].player_id,
            candidates[i].player_name,
            candidates[i].team_id,
            candidates[i].case_label,
            candidates[i].grade,
            candidates[i].score,
            candidates[i].threshold);
        detail_logs++;
    }

    append_logf(
        "KBO FA declaration event source=%s date=%u season=%u league=%u market_rows=%d market_candidates=%d active_scanned=%d active_candidates=%d candidates=%d declared=%d deferred=%d grades=%d csv=%s",
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
        grade_count,
        csv_path);

    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, market_rows);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    return 1;
}
