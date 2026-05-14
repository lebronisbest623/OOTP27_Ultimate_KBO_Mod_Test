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
        char fields[7][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 7);
        if (field_count < 7 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        uint32_t row_date = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t row_season = kbo_csv_parse_u32_text(fields[1], 10);
        uint32_t row_player_id = kbo_csv_parse_u32_text(fields[2], 10);
        if (row_player_id == player_id
                && (season == 0u || row_season == season)
                && row_date >= best_date) {
            found = 1;
            best_date = row_date;
            out_decision->player_id = row_player_id;
            out_decision->declaration_date = row_date;
            out_decision->season = row_season;
            out_decision->declared = kbo_csv_parse_u32_text(fields[4], 10) != 0u ? 1u : 0u;
            out_decision->team_id = kbo_csv_parse_u32_text(fields[5], 10);
            out_decision->league_id = kbo_csv_parse_u32_text(fields[6], 10);
        }
    }

    kbo_csv_reader_close(reader);
    return found;
}

static int kbo_get_fa_declaration_news_marker_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_declaration_news_markers.txt", out, out_size);
}

static int kbo_fa_declaration_news_marker_exists(const char* marker)
{
    if (marker == NULL || marker[0] == '\0') {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_news_marker_path(path, sizeof(path))) {
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

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL)) {
        buffer[read] = '\0';
        found = strstr(buffer, marker) != NULL;
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static void kbo_fa_declaration_news_persist_marker(const char* marker, const char* source)
{
    if (marker == NULL || marker[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_news_marker_path(path, sizeof(path))) {
        return;
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
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "KBO FA declaration news marker skipped source=%s marker=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            marker,
            GetLastError(),
            path);
        return;
    }

    DWORD written = 0;
    WriteFile(file, marker, (DWORD)strlen(marker), &written, NULL);
    WriteFile(file, "\r\n", 2u, &written, NULL);
    CloseHandle(file);
}

static int kbo_fa_declaration_news_candidate_listed(
    const uint32_t* used_ids,
    int used_count,
    uint32_t player_id)
{
    if (used_ids == NULL || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < used_count; i++) {
        if (used_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static const KboFaDeclarationCandidate* kbo_fa_declaration_news_find_best_candidate(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    const uint32_t* used_ids,
    int used_count)
{
    const KboFaDeclarationCandidate* best = NULL;
    for (int i = 0; i < candidate_count; i++) {
        const KboFaDeclarationCandidate* c = &candidates[i];
        if (c->player_id == 0u || (c->declared ? 1 : 0) != declared_filter) {
            continue;
        }
        if (kbo_fa_declaration_news_candidate_listed(used_ids, used_count, c->player_id)) {
            continue;
        }
        if (best == NULL
                || c->score > best->score
                || (c->score == best->score && c->player_id < best->player_id)) {
            best = c;
        }
    }
    return best;
}

static void kbo_fa_declaration_news_append_candidate_line(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidate,
    const char* template_key,
    const char* source)
{
    if (out == NULL || out_size == 0u || candidate == NULL || candidate->player_id == 0u) {
        return;
    }

    const char* name = candidate->player_name[0] != '\0'
        ? candidate->player_name
        : "FA candidate";
    char player_link[160] = {0};
    char team_id_text[16] = {0};
    char score_text[16] = {0};
    snprintf(player_link, sizeof(player_link), "<%s:player#%u>", name, candidate->player_id);
    snprintf(team_id_text, sizeof(team_id_text), "%u", candidate->team_id);
    snprintf(score_text, sizeof(score_text), "%d", candidate->score);

    KboNewsTemplateVar vars[] = {
        { "player_link", player_link },
        { "team_id", team_id_text },
        { "grade", candidate->grade[0] != '\0' ? candidate->grade : "-" },
        { "score", score_text },
        { "reason", candidate->decision_reason[0] != '\0' ? candidate->decision_reason : "-" },
    };

    char rendered[512] = {0};
    if (!kbo_news_template_render_key(
            template_key,
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            source)) {
        snprintf(
            rendered,
            sizeof(rendered),
            "- %s / Team #%u / %s",
            player_link,
            candidate->team_id,
            candidate->grade[0] != '\0' ? candidate->grade : "-");
    }

    kbo_news_text_append(out, out_size, rendered);
    if (!kbo_news_text_ends_with_newline(rendered)) {
        kbo_news_text_append(out, out_size, "\n");
    }
}

static void kbo_fa_declaration_news_build_candidate_list(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    int limit,
    const char* template_key,
    const char* source)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (candidates == NULL || candidate_count <= 0 || limit <= 0) {
        return;
    }

    uint32_t used_ids[16] = {0};
    int listed = 0;
    while (listed < limit && listed < (int)(sizeof(used_ids) / sizeof(used_ids[0]))) {
        const KboFaDeclarationCandidate* candidate = kbo_fa_declaration_news_find_best_candidate(
            candidates,
            candidate_count,
            declared_filter,
            used_ids,
            listed);
        if (candidate == NULL) {
            break;
        }
        used_ids[listed] = candidate->player_id;
        kbo_fa_declaration_news_append_candidate_line(out, out_size, candidate, template_key, source);
        listed++;
    }

    if (listed <= 0) {
        char none[128] = {0};
        if (!kbo_news_template_render_key(
                "fa_declaration.summary.none",
                NULL,
                0,
                none,
                sizeof(none),
                source)) {
            snprintf(none, sizeof(none), "- None");
        }
        kbo_news_text_append(out, out_size, none);
        if (!kbo_news_text_ends_with_newline(none)) {
            kbo_news_text_append(out, out_size, "\n");
        }
    }
}

static int kbo_emit_fa_declaration_summary_news(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared,
    int deferred,
    int deferred_retry,
    int deferred_no_market,
    const char* source)
{
    if (event_yyyymmdd == 0u || season < 1982u || season > 2200u || league_id == 0u
            || candidates == NULL || candidate_count <= 0) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (month == 0u || day == 0u) {
        return 0;
    }

    char marker[96] = {0};
    snprintf(marker, sizeof(marker), "summary|%u|%u", season, league_id);
    if (kbo_fa_declaration_news_marker_exists(marker)) {
        return 0;
    }

    char season_text[16] = {0};
    char declared_text[16] = {0};
    char deferred_text[16] = {0};
    char retry_text[16] = {0};
    char no_market_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", season);
    snprintf(declared_text, sizeof(declared_text), "%d", declared);
    snprintf(deferred_text, sizeof(deferred_text), "%d", deferred);
    snprintf(retry_text, sizeof(retry_text), "%d", deferred_retry);
    snprintf(no_market_text, sizeof(no_market_text), "%d", deferred_no_market);

    char declared_list[4096] = {0};
    char deferred_list[4096] = {0};
    kbo_fa_declaration_news_build_candidate_list(
        declared_list,
        sizeof(declared_list),
        candidates,
        candidate_count,
        1,
        KBO_FA_DECLARATION_NEWS_DECLARED_LIMIT,
        "fa_declaration.summary.declared_line",
        source);
    kbo_fa_declaration_news_build_candidate_list(
        deferred_list,
        sizeof(deferred_list),
        candidates,
        candidate_count,
        0,
        KBO_FA_DECLARATION_NEWS_DEFERRED_LIMIT,
        "fa_declaration.summary.deferred_line",
        source);

    KboNewsTemplateVar vars[] = {
        { "season", season_text },
        { "declared_count", declared_text },
        { "deferred_count", deferred_text },
        { "retry_count", retry_text },
        { "no_market_count", no_market_text },
        { "declared_list", declared_list },
        { "deferred_list", deferred_list },
    };

    char title[180] = {0};
    char body[8192] = {0};
    if (!kbo_news_template_render_key(
                "fa_declaration.summary.title",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                title,
                sizeof(title),
                source)
            || !kbo_news_template_render_key(
                "fa_declaration.summary.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        append_logf(
            "KBO FA declaration news skipped source=%s season=%u league_id=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_fa_declaration_news_persist_marker(marker, source);
    }
    append_logf(
        "KBO FA declaration news source=%s date=%u season=%u league=%u candidates=%d declared=%d deferred=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        season,
        league_id,
        candidate_count,
        declared,
        deferred,
        created);
    return created;
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

static int32_t kbo_fa_declaration_nonnegative_i16(int16_t value)
{
    return value > 0 ? (int32_t)value : 0;
}

static int32_t kbo_fa_declaration_current_market_score(const KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return 0;
    }
    int32_t overall = kbo_fa_declaration_nonnegative_i16(candidate->overall);
    int32_t ratings = kbo_fa_declaration_nonnegative_i16(candidate->ratings);
    int32_t career = kbo_fa_declaration_nonnegative_i16(candidate->career);
    return (overall * 45) + (ratings * 35) + (career * 20);
}

static int32_t kbo_fa_declaration_upside_score(const KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return 0;
    }
    int32_t talent = kbo_fa_declaration_nonnegative_i16(candidate->talent);
    int32_t career = kbo_fa_declaration_nonnegative_i16(candidate->career);
    int32_t overall = kbo_fa_declaration_nonnegative_i16(candidate->overall);
    int32_t ratings = kbo_fa_declaration_nonnegative_i16(candidate->ratings);
    return (talent * 45) + (career * 25) + (overall * 20) + (ratings * 10);
}

static int kbo_fa_declaration_grade_rank(const char* grade)
{
    if (grade == NULL || grade[0] == '\0') {
        return 0;
    }
    if (_stricmp(grade, "A") == 0) { return 3; }
    if (_stricmp(grade, "B") == 0) { return 2; }
    if (_stricmp(grade, "C") == 0) { return 1; }
    return 0;
}

static int kbo_fa_declaration_player_is_good(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int grade_rank)
{
    if (candidate == NULL) {
        return 0;
    }
    return grade_rank >= 2
        || candidate->score >= 72000
        || current_score >= 70000
        || upside_score >= 78000
        || (candidate->salary >= 300000000 && current_score >= 60000);
}

static int kbo_fa_declaration_should_retry_after_down_year(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int32_t form_gap,
    int grade_rank)
{
    if (candidate == NULL) {
        return 0;
    }
    int established_or_paid = grade_rank >= 2
        || candidate->score >= 65000
        || upside_score >= 76000
        || candidate->salary >= 250000000;
    return established_or_paid
        && candidate->age <= 34u
        && current_score <= 62000
        && form_gap >= 14000
        && candidate->score < 92000;
}

static int kbo_fa_declaration_should_stay_no_market(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int grade_rank,
    int player_is_good)
{
    if (candidate == NULL || player_is_good) {
        return 0;
    }

    int fringe_player = grade_rank <= 1
        && candidate->score < 64000
        && current_score < 58000
        && upside_score < 72000;
    int expensive_aging_risk = grade_rank <= 1
        && candidate->age >= 34u
        && candidate->salary >= 300000000
        && current_score < 62000;
    int weak_low_salary_market = grade_rank == 0
        && candidate->salary < 180000000
        && candidate->score < 60000
        && current_score < 56000;
    return fringe_player || expensive_aging_risk || weak_low_salary_market;
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

    int grade_rank = kbo_fa_declaration_grade_rank(candidate->grade);
    int32_t current_score = kbo_fa_declaration_current_market_score(candidate);
    int32_t upside_score = kbo_fa_declaration_upside_score(candidate);
    int32_t form_gap = upside_score > current_score ? upside_score - current_score : 0;

    int32_t threshold = 70000;
    if (grade_rank >= 3) {
        threshold = 50000;
    } else if (grade_rank == 2) {
        threshold = 56000;
    } else if (grade_rank == 1) {
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

    int player_is_good = kbo_fa_declaration_player_is_good(
        candidate,
        current_score,
        upside_score,
        grade_rank);
    if (kbo_fa_declaration_should_retry_after_down_year(
            candidate,
            current_score,
            upside_score,
            form_gap,
            grade_rank)) {
        candidate->declared = 0u;
        snprintf(
            candidate->decision_reason,
            sizeof(candidate->decision_reason),
            "ai_deferred_retry_after_down_year current=%d upside=%d gap=%d score=%d threshold=%d age=%u grade=%s salary=%d",
            current_score,
            upside_score,
            form_gap,
            candidate->score,
            candidate->threshold,
            (uint32_t)candidate->age,
            candidate->grade[0] != '\0' ? candidate->grade : "UNKNOWN",
            candidate->salary);
        return;
    }

    if (kbo_fa_declaration_should_stay_no_market(
            candidate,
            current_score,
            upside_score,
            grade_rank,
            player_is_good)) {
        candidate->declared = 0u;
        snprintf(
            candidate->decision_reason,
            sizeof(candidate->decision_reason),
            "ai_deferred_no_market_stay_original current=%d upside=%d score=%d threshold=%d age=%u grade=%s salary=%d",
            current_score,
            upside_score,
            candidate->score,
            candidate->threshold,
            (uint32_t)candidate->age,
            candidate->grade[0] != '\0' ? candidate->grade : "UNKNOWN",
            candidate->salary);
        return;
    }

    candidate->declared = (player_is_good || candidate->score >= threshold || current_score >= threshold) ? 1u : 0u;
    snprintf(
        candidate->decision_reason,
        sizeof(candidate->decision_reason),
        "%s current=%d upside=%d gap=%d score=%d threshold=%d age=%u grade=%s salary=%d",
        candidate->declared ? "ai_declared_good_or_market_fit" : "ai_deferred_no_market_stay_original",
        current_score,
        upside_score,
        form_gap,
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
        "KBO FA declaration event source=%s date=%u season=%u league=%u market_rows=%d market_candidates=%d active_scanned=%d active_candidates=%d candidates=%d declared=%d deferred=%d retry=%d no_market=%d grades=%d csv=%s",
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
