#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fa_market_historical_requalification.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../internal/fa_market_classification_internal.h"

typedef struct KboHistoricalFaSeedRecord {
    uint32_t season;
    uint32_t fa_round;
    char player_key[64];
    char grade[12];
} KboHistoricalFaSeedRecord;

#define KBO_HISTORICAL_FA_SEED_MAX 1024

static KboHistoricalFaSeedRecord g_kbo_historical_fa_seed[KBO_HISTORICAL_FA_SEED_MAX];
static int g_kbo_historical_fa_seed_count = 0;
static volatile LONG g_kbo_historical_fa_seed_loaded = 0;
static volatile LONG g_kbo_historical_fa_seed_lock = 0;

static int kbo_historical_fa_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("historical_fa_seed.csv", out, out_size);
}

static int kbo_fa_grade_is_abc(const char* grade)
{
    return grade != NULL
        && (strcmp(grade, "A") == 0 || strcmp(grade, "B") == 0 || strcmp(grade, "C") == 0);
}

static int kbo_load_historical_fa_seed_once(void)
{
    if (InterlockedCompareExchange(&g_kbo_historical_fa_seed_loaded, 0, 0) != 0) {
        return g_kbo_historical_fa_seed_count;
    }
    while (InterlockedCompareExchange(&g_kbo_historical_fa_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
    if (g_kbo_historical_fa_seed_loaded != 0) {
        InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
        return g_kbo_historical_fa_seed_count;
    }

    memset(g_kbo_historical_fa_seed, 0, sizeof(g_kbo_historical_fa_seed));
    char path[MAX_PATH] = {0};
    if (!kbo_historical_fa_seed_path(path, sizeof(path))) {
        InterlockedExchange(&g_kbo_historical_fa_seed_loaded, 1);
        InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        InterlockedExchange(&g_kbo_historical_fa_seed_loaded, 1);
        InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
        return 0;
    }

    int count = 0;
    while (count < KBO_HISTORICAL_FA_SEED_MAX && kbo_csv_reader_next_row(reader)) {
        char fields[18][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 18);
        if (field_count <= 8 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }
        uint32_t season = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t fa_round = kbo_csv_parse_u32_text(fields[1], 10);
        if (season < 1982u || season > 2200u || fa_round == 0u || fields[2][0] == '\0') {
            continue;
        }
        g_kbo_historical_fa_seed[count].season = season;
        g_kbo_historical_fa_seed[count].fa_round = fa_round;
        snprintf(g_kbo_historical_fa_seed[count].player_key, sizeof(g_kbo_historical_fa_seed[count].player_key), "%s", fields[2]);
        snprintf(g_kbo_historical_fa_seed[count].grade, sizeof(g_kbo_historical_fa_seed[count].grade), "%s", fields[8]);
        count++;
    }
    kbo_csv_reader_close(reader);
    g_kbo_historical_fa_seed_count = count;
    InterlockedExchange(&g_kbo_historical_fa_seed_loaded, 1);
    InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
    return count;
}

static const KboHistoricalFaSeedRecord* kbo_find_latest_historical_fa_seed_before(
    const char* player_key,
    uint32_t current_year)
{
    if (player_key == NULL || player_key[0] == '\0' || current_year == 0u) {
        return NULL;
    }
    int count = kbo_load_historical_fa_seed_once();
    const KboHistoricalFaSeedRecord* best = NULL;
    for (int i = 0; i < count; i++) {
        const KboHistoricalFaSeedRecord* rec = &g_kbo_historical_fa_seed[i];
        if (rec->season < current_year
                && strcmp(rec->player_key, player_key) == 0
                && (best == NULL || rec->season > best->season)) {
            best = rec;
        }
    }
    return best;
}

int kbo_fa_market_apply_requalification_grade_override(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grade,
    const KboFaRequalificationRecord* requalification_records,
    int requalification_count,
    uint32_t current_year,
    const KboFaRules* rules)
{
    if (row == NULL
            || rules == NULL
            || !kbo_fa_rules_case_is_compensable(rules, row->case_label)
            || (rules->exclude_foreign_players && row->foreign_player)) {
        return 0;
    }

    uint32_t prior_fa_count = 0u;
    const char* previous_grade = NULL;
    const char* source = NULL;
    uint32_t previous_year = 0u;
    const KboFaRequalificationRecord* rec =
        kbo_find_fa_market_requalification_record(requalification_records, requalification_count, row->player_id);
    if (rec != NULL) {
        prior_fa_count = rec->fa_count;
        previous_grade = rec->last_fa_grade;
        previous_year = rec->last_fa_year;
        source = "runtime";
    }

    if ((previous_grade == NULL || !kbo_fa_grade_is_abc(previous_grade)) && salary_grade != NULL) {
        const KboHistoricalFaSeedRecord* history =
            kbo_find_latest_historical_fa_seed_before(salary_grade->player_key, current_year);
        if (history != NULL) {
            prior_fa_count = history->fa_round;
            previous_grade = history->grade;
            previous_year = history->season;
            source = "historical_fa_seed";
        }
    }

    const char* override_grade = NULL;
    if (prior_fa_count >= 2u) {
        override_grade = "C";
    } else if (previous_grade != NULL && strcmp(previous_grade, "C") == 0) {
        override_grade = "C";
    } else if (previous_grade != NULL && (strcmp(previous_grade, "A") == 0 || strcmp(previous_grade, "B") == 0)) {
        override_grade = "B";
    }

    if (override_grade == NULL) {
        return 0;
    }

    if (_stricmp(row->grade, override_grade) != 0) {
        char previous_reason[224] = {0};
        snprintf(previous_reason, sizeof(previous_reason), "%s", row->reason);
        snprintf(row->grade, sizeof(row->grade), "%s", override_grade);
        row->fa_grade_auto = 1u;
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s; FA requalification grade=%s previous_grade=%s previous_count=%u previous_year=%u source=%s",
            previous_reason,
            override_grade,
            previous_grade != NULL && previous_grade[0] != '\0' ? previous_grade : "UNKNOWN",
            prior_fa_count,
            previous_year,
            source != NULL ? source : "unknown");
    }
    snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "REQUAL_%s", override_grade);
    return 1;
}
