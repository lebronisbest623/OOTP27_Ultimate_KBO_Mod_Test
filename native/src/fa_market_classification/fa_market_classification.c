#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bootstrap/ootp_offsets.h"
#include "../core/core_current_date.h"
#include "../core/core_league_context_parts/league_context_lookup.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../fa_filing/fa_filing.h"
#include "../fa_filing/fa_filing_parts/fa_filing_csv_parse.h"
#include "../fa_requalification/fa_requalification.h"
#include "../fa_rules/fa_rules.h"
#include "../fa_salary_snapshot/fa_salary_snapshot_parts/salary_snapshot_grade_rows.h"
#include "../foreign/foreign_waiver_date.h"
#include "../foreign/foreign_waiver_player_eval.h"
#include "../foreign/foreign_waiver_policy.h"
#include "../foreign/rights/foreign_waiver_rights_query.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_lookup.h"
#include "../team/team_name_cache.h"
#include "fa_market_classification.h"
#include "fa_market_seed_cases.h"

static uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

typedef struct KboFaMarketSqlite3 KboFaMarketSqlite3;
typedef struct KboFaMarketSqlite3Stmt KboFaMarketSqlite3Stmt;
typedef int (__cdecl *KboSqlite3OpenV2Fn)(const char*, KboFaMarketSqlite3**, int, const char*);
typedef int (__cdecl *KboSqlite3CloseFn)(KboFaMarketSqlite3*);
typedef int (__cdecl *KboSqlite3PrepareV2Fn)(KboFaMarketSqlite3*, const char*, int, KboFaMarketSqlite3Stmt**, const char**);
typedef int (__cdecl *KboSqlite3StepFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3FinalizeFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3BindIntFn)(KboFaMarketSqlite3Stmt*, int, int);
typedef int (__cdecl *KboSqlite3ResetFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3ClearBindingsFn)(KboFaMarketSqlite3Stmt*);
typedef const unsigned char* (__cdecl *KboSqlite3ColumnTextFn)(KboFaMarketSqlite3Stmt*, int);
typedef const char* (__cdecl *KboSqlite3ErrmsgFn)(KboFaMarketSqlite3*);

typedef struct KboFaMarketSqliteApi {
    HMODULE module;
    int attempted;
    int available;
    KboSqlite3OpenV2Fn open_v2;
    KboSqlite3CloseFn close;
    KboSqlite3PrepareV2Fn prepare_v2;
    KboSqlite3StepFn step;
    KboSqlite3FinalizeFn finalize;
    KboSqlite3BindIntFn bind_int;
    KboSqlite3ResetFn reset;
    KboSqlite3ClearBindingsFn clear_bindings;
    KboSqlite3ColumnTextFn column_text;
    KboSqlite3ErrmsgFn errmsg;
} KboFaMarketSqliteApi;

typedef struct KboFaMarketFileSignature {
    int exists;
    DWORD size_high;
    DWORD size_low;
    FILETIME last_write_time;
} KboFaMarketFileSignature;

static KboFaMarketSqliteApi g_kbo_fa_market_sqlite_api = {0};
static KboFaMarketHistoryCase g_kbo_fa_market_history_cache[KBO_FA_MARKET_CLASSIFICATION_MAX];
static int g_kbo_fa_market_history_cache_count = 0;
static int g_kbo_fa_market_history_cache_valid = 0;
static char g_kbo_fa_market_history_cache_save_path[MAX_PATH] = {0};
static KboFaMarketFileSignature g_kbo_fa_market_history_cache_db_sig = {0};
static KboFaMarketFileSignature g_kbo_fa_market_history_cache_wal_sig = {0};
static KboFaMarketFileSignature g_kbo_fa_market_history_cache_shm_sig = {0};

#define KBO_SQLITE_OK 0
#define KBO_SQLITE_ROW 100
#define KBO_SQLITE_DONE 101
#define KBO_SQLITE_OPEN_READONLY 0x00000001

static int kbo_get_fa_market_classification_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_market_classification.csv", out, out_size);
}

static KboFaMarketSqliteApi* kbo_fa_market_get_sqlite_api(void)
{
    if (g_kbo_fa_market_sqlite_api.attempted) {
        return g_kbo_fa_market_sqlite_api.available ? &g_kbo_fa_market_sqlite_api : NULL;
    }

    g_kbo_fa_market_sqlite_api.attempted = 1;
    HMODULE module = LoadLibraryA("winsqlite3.dll");
    if (module == NULL) {
        append_logf("FA market history sqlite unavailable reason=load_winsqlite3_failed gle=%lu", GetLastError());
        return NULL;
    }

    g_kbo_fa_market_sqlite_api.module = module;
    g_kbo_fa_market_sqlite_api.open_v2 =
        (KboSqlite3OpenV2Fn)GetProcAddress(module, "sqlite3_open_v2");
    g_kbo_fa_market_sqlite_api.close =
        (KboSqlite3CloseFn)GetProcAddress(module, "sqlite3_close");
    g_kbo_fa_market_sqlite_api.prepare_v2 =
        (KboSqlite3PrepareV2Fn)GetProcAddress(module, "sqlite3_prepare_v2");
    g_kbo_fa_market_sqlite_api.step =
        (KboSqlite3StepFn)GetProcAddress(module, "sqlite3_step");
    g_kbo_fa_market_sqlite_api.finalize =
        (KboSqlite3FinalizeFn)GetProcAddress(module, "sqlite3_finalize");
    g_kbo_fa_market_sqlite_api.bind_int =
        (KboSqlite3BindIntFn)GetProcAddress(module, "sqlite3_bind_int");
    g_kbo_fa_market_sqlite_api.reset =
        (KboSqlite3ResetFn)GetProcAddress(module, "sqlite3_reset");
    g_kbo_fa_market_sqlite_api.clear_bindings =
        (KboSqlite3ClearBindingsFn)GetProcAddress(module, "sqlite3_clear_bindings");
    g_kbo_fa_market_sqlite_api.column_text =
        (KboSqlite3ColumnTextFn)GetProcAddress(module, "sqlite3_column_text");
    g_kbo_fa_market_sqlite_api.errmsg =
        (KboSqlite3ErrmsgFn)GetProcAddress(module, "sqlite3_errmsg");

    g_kbo_fa_market_sqlite_api.available =
        g_kbo_fa_market_sqlite_api.open_v2 != NULL
        && g_kbo_fa_market_sqlite_api.close != NULL
        && g_kbo_fa_market_sqlite_api.prepare_v2 != NULL
        && g_kbo_fa_market_sqlite_api.step != NULL
        && g_kbo_fa_market_sqlite_api.finalize != NULL
        && g_kbo_fa_market_sqlite_api.bind_int != NULL
        && g_kbo_fa_market_sqlite_api.reset != NULL
        && g_kbo_fa_market_sqlite_api.clear_bindings != NULL
        && g_kbo_fa_market_sqlite_api.column_text != NULL
        && g_kbo_fa_market_sqlite_api.errmsg != NULL;
    if (!g_kbo_fa_market_sqlite_api.available) {
        append_log_line("FA market history sqlite unavailable reason=missing_winsqlite3_exports");
        return NULL;
    }
    return &g_kbo_fa_market_sqlite_api;
}

static void kbo_fa_market_text_data_source_paths(
    const char* save_path,
    char* source_db,
    size_t source_db_size,
    char* source_wal,
    size_t source_wal_size,
    char* source_shm,
    size_t source_shm_size)
{
    if (source_db != NULL && source_db_size > 0) {
        source_db[0] = '\0';
    }
    if (source_wal != NULL && source_wal_size > 0) {
        source_wal[0] = '\0';
    }
    if (source_shm != NULL && source_shm_size > 0) {
        source_shm[0] = '\0';
    }
    if (save_path == NULL || save_path[0] == '\0') {
        return;
    }
    if (source_db != NULL && source_db_size > 0) {
        snprintf(source_db, source_db_size, "%s\\temp\\text_data.sqlite3", save_path);
    }
    if (source_wal != NULL && source_wal_size > 0) {
        snprintf(source_wal, source_wal_size, "%s\\temp\\text_data.sqlite3-wal", save_path);
    }
    if (source_shm != NULL && source_shm_size > 0) {
        snprintf(source_shm, source_shm_size, "%s\\temp\\text_data.sqlite3-shm", save_path);
    }
}

static int kbo_fa_market_get_file_signature(const char* path, KboFaMarketFileSignature* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA data;
    memset(&data, 0, sizeof(data));
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)
            || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        out->exists = 0;
        return 1;
    }
    out->exists = 1;
    out->size_high = data.nFileSizeHigh;
    out->size_low = data.nFileSizeLow;
    out->last_write_time = data.ftLastWriteTime;
    return 1;
}

static int kbo_fa_market_file_signature_equal(
    const KboFaMarketFileSignature* lhs,
    const KboFaMarketFileSignature* rhs)
{
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    return lhs->exists == rhs->exists
        && lhs->size_high == rhs->size_high
        && lhs->size_low == rhs->size_low
        && lhs->last_write_time.dwHighDateTime == rhs->last_write_time.dwHighDateTime
        && lhs->last_write_time.dwLowDateTime == rhs->last_write_time.dwLowDateTime;
}

static int kbo_fa_market_get_text_data_signatures(
    const char* save_path,
    KboFaMarketFileSignature* db_sig,
    KboFaMarketFileSignature* wal_sig,
    KboFaMarketFileSignature* shm_sig)
{
    char source_db[MAX_PATH] = {0};
    char source_wal[MAX_PATH] = {0};
    char source_shm[MAX_PATH] = {0};
    kbo_fa_market_text_data_source_paths(
        save_path,
        source_db,
        sizeof(source_db),
        source_wal,
        sizeof(source_wal),
        source_shm,
        sizeof(source_shm));

    if (!kbo_fa_market_get_file_signature(source_db, db_sig)
            || !kbo_fa_market_get_file_signature(source_wal, wal_sig)
            || !kbo_fa_market_get_file_signature(source_shm, shm_sig)) {
        return 0;
    }
    return db_sig != NULL && db_sig->exists;
}

static int kbo_fa_market_history_cache_matches(
    const char* save_path,
    const KboFaMarketFileSignature* db_sig,
    const KboFaMarketFileSignature* wal_sig,
    const KboFaMarketFileSignature* shm_sig)
{
    return g_kbo_fa_market_history_cache_valid
        && save_path != NULL
        && _stricmp(g_kbo_fa_market_history_cache_save_path, save_path) == 0
        && kbo_fa_market_file_signature_equal(&g_kbo_fa_market_history_cache_db_sig, db_sig)
        && kbo_fa_market_file_signature_equal(&g_kbo_fa_market_history_cache_wal_sig, wal_sig)
        && kbo_fa_market_file_signature_equal(&g_kbo_fa_market_history_cache_shm_sig, shm_sig);
}

static int kbo_fa_market_copy_history_cache_for_rows(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return 0;
    }
    memset(histories, 0, (SIZE_T)max_histories * sizeof(histories[0]));
    int found = 0;
    for (int i = 0; i < row_count; i++) {
        histories[i].player_id = rows[i].player_id;
        for (int j = 0; j < g_kbo_fa_market_history_cache_count; j++) {
            if (g_kbo_fa_market_history_cache[j].player_id == rows[i].player_id) {
                histories[i] = g_kbo_fa_market_history_cache[j];
                if (histories[i].found) {
                    found++;
                }
                break;
            }
        }
    }
    return found;
}

static void kbo_fa_market_store_history_cache(
    const char* save_path,
    const KboFaMarketFileSignature* db_sig,
    const KboFaMarketFileSignature* wal_sig,
    const KboFaMarketFileSignature* shm_sig,
    const KboFaMarketHistoryCase* histories,
    int history_count)
{
    if (save_path == NULL || save_path[0] == '\0' || histories == NULL || history_count <= 0
            || history_count > KBO_FA_MARKET_CLASSIFICATION_MAX
            || db_sig == NULL || wal_sig == NULL || shm_sig == NULL) {
        return;
    }
    memset(g_kbo_fa_market_history_cache, 0, sizeof(g_kbo_fa_market_history_cache));
    memcpy(
        g_kbo_fa_market_history_cache,
        histories,
        (SIZE_T)history_count * sizeof(histories[0]));
    g_kbo_fa_market_history_cache_count = history_count;
    snprintf(g_kbo_fa_market_history_cache_save_path, sizeof(g_kbo_fa_market_history_cache_save_path), "%s", save_path);
    g_kbo_fa_market_history_cache_db_sig = *db_sig;
    g_kbo_fa_market_history_cache_wal_sig = *wal_sig;
    g_kbo_fa_market_history_cache_shm_sig = *shm_sig;
    g_kbo_fa_market_history_cache_valid = 1;
}

static int kbo_fa_market_copy_file_if_present(const char* source, const char* destination)
{
    if (source == NULL || destination == NULL || source[0] == '\0' || destination[0] == '\0') {
        return 0;
    }
    DWORD attributes = GetFileAttributesA(source);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        DeleteFileA(destination);
        return 1;
    }
    if (CopyFileA(source, destination, FALSE)) {
        return 1;
    }
    append_logf("FA market history sqlite copy failed src=%s dst=%s gle=%lu", source, destination, GetLastError());
    return 0;
}

static int kbo_fa_market_copy_text_data_sqlite(char* out_path, size_t out_path_size)
{
    if (out_path == NULL || out_path_size == 0) {
        return 0;
    }
    out_path[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        append_log_line("FA market history sqlite skipped reason=no_current_save_path");
        return 0;
    }

    char data_dir[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_dir(data_dir, sizeof(data_dir))) {
        append_log_line("FA market history sqlite skipped reason=no_save_scoped_data_dir");
        return 0;
    }

    char source_db[MAX_PATH] = {0};
    char source_wal[MAX_PATH] = {0};
    char source_shm[MAX_PATH] = {0};
    kbo_fa_market_text_data_source_paths(
        save_path,
        source_db,
        sizeof(source_db),
        source_wal,
        sizeof(source_wal),
        source_shm,
        sizeof(source_shm));

    if (GetFileAttributesA(source_db) == INVALID_FILE_ATTRIBUTES) {
        append_logf("FA market history sqlite skipped reason=source_missing path=%s", source_db);
        return 0;
    }

    DWORD pid = GetCurrentProcessId();
    char dest_db[MAX_PATH] = {0};
    char dest_wal[MAX_PATH] = {0};
    char dest_shm[MAX_PATH] = {0};
    snprintf(dest_db, sizeof(dest_db), "%s\\fa_market_text_data_%lu.sqlite3", data_dir, (unsigned long)pid);
    snprintf(dest_wal, sizeof(dest_wal), "%s-wal", dest_db);
    snprintf(dest_shm, sizeof(dest_shm), "%s-shm", dest_db);

    if (!kbo_fa_market_copy_file_if_present(source_db, dest_db)) {
        return 0;
    }
    kbo_fa_market_copy_file_if_present(source_wal, dest_wal);
    kbo_fa_market_copy_file_if_present(source_shm, dest_shm);

    snprintf(out_path, out_path_size, "%s", dest_db);
    return out_path[0] != '\0';
}

static void kbo_fa_market_copy_sqlite_text_column(
    KboFaMarketSqliteApi* api,
    KboFaMarketSqlite3Stmt* stmt,
    int column,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (api == NULL || stmt == NULL || api->column_text == NULL) {
        return;
    }
    const unsigned char* text = api->column_text(stmt, column);
    if (text == NULL) {
        return;
    }
    snprintf(out, out_size, "%s", (const char*)text);
}

static const KboFaRequalificationRecord* kbo_find_fa_market_requalification_record(
    const KboFaRequalificationRecord* records,
    int record_count,
    uint32_t player_id)
{
    if (records == NULL || record_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < record_count; i++) {
        if (records[i].player_id == player_id) {
            return &records[i];
        }
    }
    return NULL;
}

static uint32_t kbo_fa_market_resolve_league_id(uint32_t requested_league_id)
{
    (void)requested_league_id;

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id != 0u) {
        return league_id;
    }
    return kbo_resolve_kbo_league_id();
}

static uint32_t kbo_fa_market_get_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static int kbo_fa_market_team_belongs_to_league(uint32_t team_id, uint32_t league_id)
{
    if (team_id == 0u || league_id == 0u) {
        return 0;
    }
    return kbo_fa_market_get_team_league_id(team_id) == league_id;
}

static int kbo_fa_market_player_has_kbo_pro_context(uint8_t* player, uint32_t league_id)
{
    if (player == NULL || league_id == 0u) {
        return 1;
    }

    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (current_league_id == league_id || draft_league_id == league_id) {
        return 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == 0u
            && kbo_player_has_nonzero_evaluation(player)) {
        return 1;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = kbo_get_player_original_team_id(player);
    return kbo_fa_market_team_belongs_to_league(active_team_id, league_id)
        || kbo_fa_market_team_belongs_to_league(original_team_id, league_id);
}

static int kbo_fa_market_player_is_candidate(uint8_t* player, uint32_t league_id)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];

    if (player_id == 0u || current_team_id != 0u || retired_flag != 0u || age < 16u || age > 60u) {
        return 0;
    }
    if (kbo_player_is_draft_pool_candidate(player)) {
        return 0;
    }
    return kbo_fa_market_player_has_kbo_pro_context(player, league_id);
}

static int kbo_fa_market_row_is_undrafted_domestic(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->generation_context != 0u
            || row->generation_grade == 0u) {
        return 0;
    }

    if (row->draft_league_id == 201u && row->draft_subtype == 1u && row->age <= 25u) {
        return 1;
    }
    if (row->draft_league_id == 200u && row->age <= 20u) {
        return 1;
    }
    return 0;
}

static int kbo_fa_market_row_is_independent_league_fa(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->original_team_id == 0u) {
        return 0;
    }

    if (kbo_fa_market_row_is_undrafted_domestic(row)) {
        return 0;
    }

    uint32_t original_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
    if (original_league_id == KBO_FA_MARKET_INDEPENDENT_LEAGUE_ID) {
        return 1;
    }
    return original_league_id == 0u && row->draft_league_id == KBO_FA_MARKET_INDEPENDENT_LEAGUE_ID;
}

static void kbo_fa_market_set_history_reason(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    const char* prefix)
{
    if (row == NULL) {
        return;
    }
    if (history != NULL && history->history_date[0] != '\0' && history->history_text[0] != '\0') {
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s history=%s %.96s",
            prefix != NULL ? prefix : "player history match",
            history->history_date,
            history->history_text);
        return;
    }
    snprintf(row->reason, sizeof(row->reason), "%s", prefix != NULL ? prefix : "player history match");
}

static int kbo_fa_market_apply_history_case(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history)
{
    if (row == NULL || history == NULL || !history->found) {
        return 0;
    }

    if (history->undrafted_free_agent) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        kbo_fa_market_set_history_reason(row, history, "player history says undrafted free agent");
        return 1;
    }

    if (history->released) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_RELEASED_NON_FA");
        kbo_fa_market_set_history_reason(row, history, "player history says released, not official FA");
        return 1;
    }

    if (history->became_free_agent) {
        if (kbo_fa_market_row_is_independent_league_fa(row)) {
            snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_LEAGUE_FA");
            kbo_fa_market_set_history_reason(row, history, "player history says free agent from independent-league context");
            return 1;
        }

        snprintf(row->case_label, sizeof(row->case_label), "KBO_FA_BY_HISTORY_UNGRADED");
        kbo_fa_market_set_history_reason(row, history, "player history says became a free agent; grade pending salary snapshot or seed");
        return 1;
    }

    return 0;
}

static int kbo_fa_market_grade_is_unknown(const char* grade)
{
    return grade == NULL
        || grade[0] == '\0'
        || _stricmp(grade, "UNKNOWN") == 0
        || strcmp(grade, "-") == 0;
}

const char* kbo_fa_market_display_grade(const char* grade)
{
    return kbo_fa_market_grade_is_unknown(grade) ? "-" : grade;
}

uint32_t kbo_fa_market_display_grade_sort_rank(const char* grade)
{
    if (grade != NULL && _stricmp(grade, "A") == 0) { return 1u; }
    if (grade != NULL && _stricmp(grade, "B") == 0) { return 2u; }
    if (grade != NULL && _stricmp(grade, "C") == 0) { return 3u; }
    return 4u;
}

uint32_t kbo_fa_market_display_team_id(const KboFaMarketClassification* row)
{
    if (row == NULL) {
        return 0u;
    }
    if (row->fa_grade_snapshot_team_id != 0u) {
        return row->fa_grade_snapshot_team_id;
    }
    if (row->original_team_id != 0u) {
        return row->original_team_id;
    }
    if (row->active_team_id != 0u) {
        return row->active_team_id;
    }
    return row->current_team_id;
}

void kbo_fa_market_format_salary(int32_t salary, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (salary <= 0) {
        snprintf(out, out_size, "-");
        return;
    }

    char raw[32] = {0};
    char formatted[48] = {0};
    snprintf(raw, sizeof(raw), "%d", salary);
    size_t raw_len = strlen(raw);
    size_t pos = 0;
    for (size_t i = 0; i < raw_len && pos + 1 < sizeof(formatted); i++) {
        if (i > 0 && ((raw_len - i) % 3u) == 0u && pos + 1 < sizeof(formatted)) {
            formatted[pos++] = ',';
        }
        formatted[pos++] = raw[i];
    }
    formatted[pos] = '\0';
    snprintf(out, out_size, "%s", formatted);
}

const char* kbo_fa_market_display_case_label(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return "-";
    }
    if (strcmp(case_label, "KBO_FA_APPROVED") == 0) { return "Official FA"; }
    if (strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) { return "Eligible FA"; }
    if (strcmp(case_label, "KBO_FA_DEFERRED") == 0) { return "Deferred FA"; }
    if (strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0) { return "FA"; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_LOCKED") == 0) { return "Requal Lock"; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0) { return "Requal FA"; }
    if (strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0) { return "Released"; }
    if (strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0) { return "Undrafted"; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0) { return "Indy FA"; }
    if (strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0) { return "Teamless"; }
    if (strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) { return "Teamless"; }
    if (strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0) { return "Foreign Rights"; }
    if (strcmp(case_label, "FOREIGN_FREE") == 0) { return "Foreign FA"; }
    return case_label;
}

static int kbo_fa_market_apply_age_grade_override(KboFaMarketClassification* row, const KboFaRules* rules)
{
    if (row == NULL
            || rules == NULL
            || !rules->age_grade_override_enabled
            || rules->age_grade_min_age == 0u
            || rules->age_grade[0] == '\0'
            || (rules->exclude_foreign_players && row->foreign_player)
            || row->age < rules->age_grade_min_age
            || !kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return 0;
    }

    if (_stricmp(row->grade, rules->age_grade) != 0) {
        snprintf(row->grade, sizeof(row->grade), "%s", rules->age_grade);
        row->fa_grade_auto = 1u;
        char previous_reason[224] = {0};
        snprintf(previous_reason, sizeof(previous_reason), "%s", row->reason);
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s; age >= %u FA grade override=%s",
            previous_reason,
            rules->age_grade_min_age,
            rules->age_grade);
    }
    snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AGE_%u_%s", rules->age_grade_min_age, rules->age_grade);
    return 1;
}

void kbo_fa_market_apply_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRules* rules)
{
    KboFaRules local_rules;
    if (rules == NULL) {
        kbo_fa_rules_load(&local_rules);
        rules = &local_rules;
    }

    if (row == NULL || !kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return;
    }
    if ((rules->exclude_foreign_players && row->foreign_player)
            || strcmp(row->case_label, "FOREIGN_FREE") == 0
            || strcmp(row->case_label, "FOREIGN_RESERVED_RIGHT") == 0) {
        return;
    }

    int age_grade_override = kbo_fa_market_apply_age_grade_override(row, rules);

    const KboFaSalarySnapshotGrade* grade =
        kbo_find_fa_salary_snapshot_grade(salary_grades, salary_grade_count, row->player_id);
    if (grade == NULL) {
        if (!age_grade_override) {
            snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "SNAPSHOT_MISSING");
        }
        return;
    }

    row->fa_grade_salary = grade->salary;
    row->fa_grade_overall_rank = grade->overall_rank;
    row->fa_grade_team_rank = grade->team_rank;
    row->fa_grade_snapshot_team_id = grade->ranking_team_id;
    row->fa_grade_snapshot_date = grade->snapshot_date;
    row->fa_grade_opening_day = grade->opening_day;
    if (!age_grade_override
            && kbo_fa_market_grade_is_unknown(row->grade)
            && !kbo_fa_market_grade_is_unknown(grade->grade)) {
        snprintf(row->grade, sizeof(row->grade), "%s", grade->grade);
        row->fa_grade_auto = 1u;
        char previous_reason[224] = {0};
        snprintf(previous_reason, sizeof(previous_reason), "%s", row->reason);
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s; opening-day salary grade=%s salary=%d overall_rank=%u team_rank=%u",
            previous_reason,
            row->grade,
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank);
    }

    if (row->original_team_id != 0u
            && grade->ranking_team_id != 0u
            && row->original_team_id != grade->ranking_team_id) {
        row->fa_grade_team_changed_review = 1u;
        snprintf(
            row->fa_grade_flag,
            sizeof(row->fa_grade_flag),
            "TEAM_CHANGED_REVIEW");
    } else if (age_grade_override) {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AGE_%u_%s", rules->age_grade_min_age, rules->age_grade);
    } else if (row->fa_grade_auto) {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AUTO");
    } else {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "SNAPSHOT");
    }
}

static void kbo_fa_market_mark_history_case(KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_text[0] == '\0') {
        return;
    }
    history->became_free_agent = strcmp(history->history_text, "[G]Became a free agent.") == 0;
    history->undrafted_free_agent =
        strstr(history->history_text, "Was not drafted and became a free agent") != NULL;
    history->released = strncmp(history->history_text, "[G]Released by ", 15) == 0;
}

int kbo_load_fa_market_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return 0;
    }
    memset(histories, 0, (SIZE_T)max_histories * sizeof(histories[0]));
    for (int i = 0; i < row_count; i++) {
        histories[i].player_id = rows[i].player_id;
    }

    char save_path[MAX_PATH] = {0};
    KboFaMarketFileSignature db_sig = {0};
    KboFaMarketFileSignature wal_sig = {0};
    KboFaMarketFileSignature shm_sig = {0};
    int have_signature = 0;
    if (kbo_get_current_save_path(save_path, sizeof(save_path))) {
        have_signature = kbo_fa_market_get_text_data_signatures(save_path, &db_sig, &wal_sig, &shm_sig);
        if (have_signature && kbo_fa_market_history_cache_matches(save_path, &db_sig, &wal_sig, &shm_sig)) {
            int cached_found = kbo_fa_market_copy_history_cache_for_rows(rows, row_count, histories, max_histories);
            append_logf("FA market history sqlite cache hit found=%d rows=%d save=%s", cached_found, row_count, save_path);
            return cached_found;
        }
    }

    KboFaMarketSqliteApi* api = kbo_fa_market_get_sqlite_api();
    if (api == NULL) {
        return 0;
    }

    char db_path[MAX_PATH] = {0};
    if (!kbo_fa_market_copy_text_data_sqlite(db_path, sizeof(db_path))) {
        return 0;
    }

    KboFaMarketSqlite3* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != KBO_SQLITE_OK || db == NULL) {
        append_logf(
            "FA market history sqlite open failed result=%d path=%s msg=%s",
            open_result,
            db_path,
            (db != NULL && api->errmsg != NULL) ? api->errmsg(db) : "");
        if (db != NULL) {
            api->close(db);
        }
        return 0;
    }

    const char* sql =
        "SELECT history_date, history_text "
        "FROM player_history "
        "WHERE player_id=?1 AND ("
        "history_text='[G]Became a free agent.' "
        "OR history_text LIKE '[G]Was not drafted and became a free agent%' "
        "OR history_text LIKE '[G]Released by %') "
        "ORDER BY history_date DESC, history_id DESC LIMIT 1;";

    KboFaMarketSqlite3Stmt* stmt = NULL;
    int prepare_result = api->prepare_v2(db, sql, -1, &stmt, NULL);
    if (prepare_result != KBO_SQLITE_OK || stmt == NULL) {
        append_logf(
            "FA market history sqlite prepare failed result=%d msg=%s",
            prepare_result,
            api->errmsg != NULL ? api->errmsg(db) : "");
        api->close(db);
        return 0;
    }

    int found = 0;
    for (int i = 0; i < row_count; i++) {
        api->reset(stmt);
        api->clear_bindings(stmt);
        api->bind_int(stmt, 1, (int)rows[i].player_id);
        int step_result = api->step(stmt);
        if (step_result == KBO_SQLITE_ROW) {
            histories[i].found = 1;
            kbo_fa_market_copy_sqlite_text_column(api, stmt, 0, histories[i].history_date, sizeof(histories[i].history_date));
            kbo_fa_market_copy_sqlite_text_column(api, stmt, 1, histories[i].history_text, sizeof(histories[i].history_text));
            kbo_fa_market_mark_history_case(&histories[i]);
            found++;
        } else if (step_result != KBO_SQLITE_DONE) {
            append_logf(
                "FA market history sqlite step failed player=%u result=%d msg=%s",
                rows[i].player_id,
                step_result,
                api->errmsg != NULL ? api->errmsg(db) : "");
        }
    }

    api->finalize(stmt);
    api->close(db);
    append_logf("FA market history sqlite loaded found=%d rows=%d db=%s", found, row_count, db_path);
    if (have_signature) {
        kbo_fa_market_store_history_cache(save_path, &db_sig, &wal_sig, &shm_sig, histories, row_count);
    }
    return found;
}

static const KboFaMarketHistoryCase* kbo_find_fa_market_history_case(
    const KboFaMarketHistoryCase* histories,
    int history_count,
    uint32_t player_id)
{
    if (histories == NULL || history_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < history_count; i++) {
        if (histories[i].player_id == player_id && histories[i].found) {
            return &histories[i];
        }
    }
    return NULL;
}

static uint32_t kbo_fa_market_history_date_u32(const KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_date[0] == '\0') {
        return 0u;
    }
    uint32_t date = 0u;
    if (kbo_parse_yyyymmdd(history->history_date, &date)) {
        return date;
    }
    return kbo_fa_filing_parse_u32(history->history_date);
}

int kbo_fa_market_overlay_filing_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int history_count)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || history_count < row_count) {
        return 0;
    }

    KboFaFilingRecord* filings = (KboFaFilingRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
    if (filings == NULL) {
        append_log_line("FA market filing ledger overlay skipped: allocation failed");
        return 0;
    }

    char path[MAX_PATH] = {0};
    int filing_count = kbo_load_fa_filing_records(filings, KBO_FA_FILING_MAX, path, sizeof(path));
    int applied = 0;
    for (int f = 0; f < filing_count; f++) {
        const KboFaFilingRecord* filing = &filings[f];
        if (filing->player_id == 0u || filing->filing_date == 0u) {
            continue;
        }
        for (int i = 0; i < row_count; i++) {
            if (rows[i].player_id != filing->player_id) {
                continue;
            }
            uint32_t existing_date = kbo_fa_market_history_date_u32(&histories[i]);
            if (histories[i].found && existing_date > filing->filing_date) {
                break;
            }

            histories[i].player_id = rows[i].player_id;
            histories[i].found = 1;
            histories[i].became_free_agent = 1;
            histories[i].undrafted_free_agent = 0;
            histories[i].released = 0;
            snprintf(histories[i].history_date, sizeof(histories[i].history_date), "%u", filing->filing_date);
            snprintf(histories[i].history_text, sizeof(histories[i].history_text), "[G]Became a free agent.");
            if (rows[i].original_team_id == 0u && filing->original_team_id != 0u) {
                rows[i].original_team_id = filing->original_team_id;
            }
            applied++;
            break;
        }
    }

    if (filing_count > 0 || applied > 0) {
        append_logf(
            "FA market filing ledger overlay applied=%d filings=%d rows=%d csv=%s",
            applied,
            filing_count,
            row_count,
            path);
    }
    HeapFree(GetProcessHeap(), 0, filings);
    return applied;
}

void kbo_classify_fa_market_row(
    KboFaMarketClassification* row,
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    const KboFaRequalificationRecord* records,
    int record_count,
    const KboFaMarketHistoryCase* history_case,
    uint32_t current_year,
    uint32_t today_yyyymmdd)
{
    if (row == NULL) {
        return;
    }

    snprintf(row->grade, sizeof(row->grade), "UNKNOWN");
    row->case_label[0] = '\0';
    row->reason[0] = '\0';

    const KboFaMarketSeedCase* seed = kbo_find_fa_market_seed_case(seeds, seed_count, row->player_id);
    if (seed != NULL) {
        const char* seed_case = kbo_fa_market_canonical_case_label(seed->case_label);
        snprintf(row->case_label, sizeof(row->case_label), "%s", seed_case != NULL ? seed_case : seed->case_label);
        snprintf(row->grade, sizeof(row->grade), "%s", seed->grade[0] != '\0' ? seed->grade : "UNKNOWN");
        if (seed->note[0] != '\0') {
            snprintf(row->reason, sizeof(row->reason), "%s", seed->note);
        } else if (kbo_fa_market_case_is_seeded_official(seed_case)) {
            snprintf(row->reason, sizeof(row->reason), "manual official FA seed");
        } else {
            snprintf(row->reason, sizeof(row->reason), "manual case seed");
        }
        return;
    }

    if (row->foreign_player) {
        uint32_t rights_team_id = 0u;
        if (today_yyyymmdd != 0u && kbo_find_active_foreign_waiver_holder(row->player_id, today_yyyymmdd, &rights_team_id)) {
            row->rights_team_id = rights_team_id;
            snprintf(row->case_label, sizeof(row->case_label), "FOREIGN_RESERVED_RIGHT");
            snprintf(row->reason, sizeof(row->reason), "active foreign reserve right held by team #%u", rights_team_id);
            return;
        }
        snprintf(row->case_label, sizeof(row->case_label), "FOREIGN_FREE");
        snprintf(row->reason, sizeof(row->reason), "foreign player with no active KBO reserve right");
        return;
    }

    const KboFaRequalificationRecord* rec =
        kbo_find_fa_market_requalification_record(records, record_count, row->player_id);
    if (rec != NULL) {
        uint32_t eligible_year = rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS;
        if (row->original_team_id == 0u) {
            row->original_team_id = rec->original_team_id;
        }
        if (current_year != 0u && current_year < eligible_year) {
            snprintf(row->case_label, sizeof(row->case_label), "KBO_REQUALIFICATION_LOCKED");
            snprintf(
                row->reason,
                sizeof(row->reason),
                "last_fa=%u eligible=%u count=%u original_team=%u",
                rec->last_fa_year,
                eligible_year,
                rec->fa_count,
                rec->original_team_id);
            return;
        }
        snprintf(row->case_label, sizeof(row->case_label), "KBO_REQUALIFICATION_ELIGIBLE");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "timer complete last_fa=%u eligible=%u; approval/declaration still needs manual seed",
            rec->last_fa_year,
            eligible_year);
        return;
    }

    if (kbo_fa_market_apply_history_case(row, history_case)) {
        return;
    }

    if (kbo_fa_market_row_is_undrafted_domestic(row)) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "domestic undrafted free agent marker draft_league=%u subtype=%u gen=%u/%u",
            row->draft_league_id,
            (uint32_t)row->draft_subtype,
            (uint32_t)row->generation_context,
            (uint32_t)row->generation_grade);
        return;
    }

    if (kbo_fa_market_row_is_independent_league_fa(row)) {
        uint32_t original_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_LEAGUE_FA");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "domestic independent-league free agent marker original_league=%u original_team=%u draft_league=%u",
            original_league_id,
            row->original_team_id,
            row->draft_league_id);
        return;
    }

    snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_TEAMLESS_UNVERIFIED");
    snprintf(row->reason, sizeof(row->reason), "domestic active teamless player; release/official FA origin not proven without seed");
}

static int kbo_fa_market_case_rank(const char* case_label)
{
    if (case_label == NULL) {
        return 99;
    }
    if (strcmp(case_label, "KBO_FA_APPROVED") == 0) { return 10; }
    if (strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) { return 11; }
    if (strcmp(case_label, "KBO_FA_DEFERRED") == 0) { return 12; }
    if (strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0) { return 13; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_LOCKED") == 0) { return 20; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0) { return 21; }
    if (strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0) { return 30; }
    if (strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0) { return 31; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0) { return 32; }
    if (strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0) { return 33; }
    if (strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) { return 33; }
    if (strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0) { return 40; }
    if (strcmp(case_label, "FOREIGN_FREE") == 0) { return 41; }
    return 99;
}

static int kbo_compare_fa_market_classification_rows(const void* lhs, const void* rhs)
{
    const KboFaMarketClassification* a = (const KboFaMarketClassification*)lhs;
    const KboFaMarketClassification* b = (const KboFaMarketClassification*)rhs;
    int rank_a = kbo_fa_market_case_rank(a->case_label);
    int rank_b = kbo_fa_market_case_rank(b->case_label);
    if (rank_a != rank_b) {
        return rank_a - rank_b;
    }
    int name_cmp = _stricmp(a->player_name, b->player_name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    if (a->player_id < b->player_id) { return -1; }
    if (a->player_id > b->player_id) { return 1; }
    return 0;
}

static void kbo_fa_market_write_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

static void kbo_fa_market_write_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    kbo_fa_market_write_raw(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            char ch = *p;
            if (ch == '"') {
                kbo_fa_market_write_raw(file, "\"\"");
            } else {
                char one[2] = { ch, '\0' };
                kbo_fa_market_write_raw(file, one);
            }
        }
    }
    kbo_fa_market_write_raw(file, "\"");
}

static void kbo_write_fa_market_classification_csv(
    const KboFaMarketClassification* rows,
    int row_count,
    const KboFaMarketScanSummary* summary,
    const char* source)
{
    if (rows == NULL || row_count < 0 || summary == NULL) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_market_classification_csv_path(path, sizeof(path))) {
        append_log_line("FA market classification: unable to resolve CSV path");
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("FA market classification: failed to open CSV path=%s gle=%lu", path, GetLastError());
        return;
    }

    kbo_fa_market_write_raw(
        file,
        "date,source,selected_league_id,player_id,name,nation_id,current_team_id,active_team_id,original_team_id,current_league_id,draft_league_id,age,retired_flag,contract_level,fa_demand,dfa_flag,foreign_flag,draft_class,draft_subtype,draft_eligible,draft_extra,generation_flags,generation_context,generation_grade,generation_special,rights_team_id,kbo_case,kbo_grade,fa_grade_salary,fa_grade_overall_rank,fa_grade_team_rank,fa_grade_snapshot_team_id,fa_grade_snapshot_date,fa_grade_opening_day,fa_grade_auto,fa_grade_team_changed_review,fa_grade_flag,reason\r\n");

    char date[16] = {0};
    if (summary->today_yyyymmdd != 0u) {
        snprintf(date, sizeof(date), "%08u", summary->today_yyyymmdd);
    } else if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        snprintf(date, sizeof(date), "00000000");
    }

    for (int i = 0; i < row_count; i++) {
        const KboFaMarketClassification* row = &rows[i];
        char prefix[256] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%s,%s,%u,%u,",
            date,
            source != NULL ? source : "",
            summary->league_id,
            row->player_id);
        if (len <= 0 || len >= (int)sizeof(prefix)) {
            continue;
        }
        kbo_fa_market_write_raw(file, prefix);
        kbo_fa_market_write_csv_text(file, row->player_name);
        char middle[320] = {0};
        len = snprintf(
            middle,
            sizeof(middle),
            ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,",
            row->nation_id,
            row->current_team_id,
            row->active_team_id,
            row->original_team_id,
            row->current_league_id,
            row->draft_league_id,
            (uint32_t)row->age,
            (uint32_t)row->retired_flag,
            (uint32_t)row->contract_level,
            row->fa_demand,
            (uint32_t)row->dfa,
            (uint32_t)row->foreign_player,
            (uint32_t)row->draft_class,
            (uint32_t)row->draft_subtype,
            (uint32_t)row->draft_eligible,
            (uint32_t)row->draft_extra,
            (uint32_t)row->generation_flags,
            (uint32_t)row->generation_context,
            (uint32_t)row->generation_grade,
            (uint32_t)row->generation_special,
            row->rights_team_id);
        if (len <= 0 || len >= (int)sizeof(middle)) {
            continue;
        }
        kbo_fa_market_write_raw(file, middle);
        kbo_fa_market_write_csv_text(file, row->case_label);
        kbo_fa_market_write_raw(file, ",");
        kbo_fa_market_write_csv_text(file, row->grade);
        char grade_middle[192] = {0};
        len = snprintf(
            grade_middle,
            sizeof(grade_middle),
            ",%d,%u,%u,%u,%u,%u,%u,%u,",
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank,
            row->fa_grade_snapshot_team_id,
            row->fa_grade_snapshot_date,
            row->fa_grade_opening_day,
            (uint32_t)row->fa_grade_auto,
            (uint32_t)row->fa_grade_team_changed_review);
        if (len <= 0 || len >= (int)sizeof(grade_middle)) {
            continue;
        }
        kbo_fa_market_write_raw(file, grade_middle);
        kbo_fa_market_write_csv_text(file, row->fa_grade_flag);
        kbo_fa_market_write_raw(file, ",");
        kbo_fa_market_write_csv_text(file, row->reason);
        kbo_fa_market_write_raw(file, "\r\n");
    }

    CloseHandle(file);
    append_logf(
        "FA market classification: rows=%d candidates=%d scanned=%d league=%u salary_snapshot=%d csv=%s",
        row_count,
        summary->candidates,
        summary->scanned,
        summary->league_id,
        summary->salary_snapshot_count,
        path);
}

int kbo_collect_fa_market_classifications(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source)
{
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));
    if (summary != NULL) {
        memset(summary, 0, sizeof(*summary));
    }

    uint32_t league_id = kbo_fa_market_resolve_league_id(requested_league_id);
    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);
    uint32_t current_year = 0u;
    kbo_current_year_relaxed(&current_year);

    if (summary != NULL) {
        summary->league_id = league_id;
        summary->today_yyyymmdd = today;
        summary->current_year = current_year;
        kbo_get_fa_market_classification_csv_path(summary->csv_path, sizeof(summary->csv_path));
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_log_line("FA market classification: no player vector");
        return 0;
    }

    KboFaMarketSeedCase* seeds = (KboFaMarketSeedCase*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_SEED_MAX * sizeof(KboFaMarketSeedCase));
    KboFaRequalificationRecord* records = (KboFaRequalificationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_REQUALIFICATION_MAX * sizeof(KboFaRequalificationRecord));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (seeds == NULL || records == NULL || salary_grades == NULL) {
        if (seeds != NULL) {
            HeapFree(GetProcessHeap(), 0, seeds);
        }
        if (records != NULL) {
            HeapFree(GetProcessHeap(), 0, records);
        }
        if (salary_grades != NULL) {
            HeapFree(GetProcessHeap(), 0, salary_grades);
        }
        append_log_line("FA market classification: allocation failed");
        return 0;
    }

    char seed_path[MAX_PATH] = {0};
    char salary_snapshot_path[MAX_PATH] = {0};
    int seed_count = kbo_load_fa_market_seed_cases(seeds, KBO_FA_MARKET_SEED_MAX, seed_path, sizeof(seed_path));
    int requalification_count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int salary_grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        current_year,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        salary_snapshot_path,
        sizeof(salary_snapshot_path));
    KboFaRules fa_rules;
    kbo_fa_rules_load(&fa_rules);
    if (summary != NULL) {
        summary->seed_count = seed_count;
        summary->requalification_count = requalification_count;
        summary->salary_snapshot_count = salary_grade_count;
        snprintf(summary->seed_path, sizeof(summary->seed_path), "%s", seed_path);
        snprintf(summary->salary_snapshot_path, sizeof(summary->salary_snapshot_path), "%s", salary_snapshot_path);
    }

    int row_count = 0;
    int candidate_count = 0;
    int scanned = 0;
    int truncated = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_fa_market_player_is_candidate(player, league_id)) {
            continue;
        }
        candidate_count++;
        if (row_count >= max_rows) {
            truncated = 1;
            continue;
        }

        KboFaMarketClassification* row = &rows[row_count];
        row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        row->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        row->original_team_id = kbo_get_player_original_team_id(player);
        row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
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
        row_count++;
    }

    KboFaMarketHistoryCase* histories = NULL;
    int history_count = 0;
    if (row_count > 0) {
        histories = (KboFaMarketHistoryCase*)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            (SIZE_T)row_count * sizeof(KboFaMarketHistoryCase));
        if (histories != NULL) {
            kbo_load_fa_market_history_cases(rows, row_count, histories, row_count);
            kbo_fa_market_overlay_filing_history_cases(rows, row_count, histories, row_count);
            history_count = row_count;
        } else {
            append_log_line("FA market classification: history allocation failed");
        }
    }

    for (int i = 0; i < row_count; i++) {
        const KboFaMarketHistoryCase* history_case =
            kbo_find_fa_market_history_case(histories, history_count, rows[i].player_id);
        kbo_classify_fa_market_row(
            &rows[i],
            seeds,
            seed_count,
            records,
            requalification_count,
            history_case,
            current_year,
            today);
        kbo_fa_market_apply_salary_snapshot_grade(&rows[i], salary_grades, salary_grade_count, &fa_rules);
    }

    if (row_count > 1) {
        qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_compare_fa_market_classification_rows);
    }

    if (summary != NULL) {
        summary->scanned = scanned;
        summary->candidates = candidate_count;
        summary->rows = row_count;
        summary->truncated = truncated;
    }

    if (write_csv) {
        kbo_write_fa_market_classification_csv(rows, row_count, summary, source);
        if (summary != NULL) {
            summary->csv_written = 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, seeds);
    HeapFree(GetProcessHeap(), 0, records);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    if (histories != NULL) {
        HeapFree(GetProcessHeap(), 0, histories);
    }
    return row_count;
}
