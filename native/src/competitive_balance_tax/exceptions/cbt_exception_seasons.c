#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbt_exceptions.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/spin_lock.h"
#include "../../fa_salary_snapshot/csv/salary_snapshot_csv_parse.h"
#include "../../hotkey_window/support/assets/names/support_names.h"

typedef struct KboCbtSeasonSeedRow {
    char player_key[64];
    char team_code[16];
    int season_count;
} KboCbtSeasonSeedRow;

static KboCbtSeasonSeedRow g_cbt_season_seed[KBO_CBT_EXCEPTION_SEASON_SEED_MAX];
static int g_cbt_season_seed_count = 0;
static volatile LONG g_cbt_season_seed_loaded = 0;
static volatile LONG g_cbt_season_seed_lock = 0;

static void kbo_cbt_exception_lock(volatile LONG* lock)
{
    kbo_spin_lock(lock);
}

static void kbo_cbt_exception_unlock(volatile LONG* lock)
{
    kbo_spin_unlock(lock);
}

static int kbo_cbt_exception_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file("cbt_player_team_seasons_seed.csv", out, out_size);
}

static int kbo_cbt_exception_bundled_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    HMODULE module = GetModuleHandleA("KBOFix.dll");
    if (module == NULL) {
        module = GetModuleHandleA(NULL);
    }

    char path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameA(module, path, (DWORD)sizeof(path));
    if (len == 0u || len >= (DWORD)sizeof(path)) {
        return 0;
    }
    char* slash = strrchr(path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *(slash + 1) = '\0';

    int written = snprintf(out, out_size, "%sdata\\seeds\\cbt_player_team_seasons_seed.csv", path);
    return written > 0 && written < (int)out_size;
}

static void kbo_cbt_exception_copy_team_seed_code(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    kbo_hub_copy_team_abbrev_by_id(team_id, out, out_size, "");
    if (_stricmp(out, "HAN") == 0) {
        snprintf(out, out_size, "HH");
    } else if (_stricmp(out, "KT") == 0) {
        snprintf(out, out_size, "kt");
    } else {
        size_t n = strlen(out);
        if (n > 1u && out[n - 1u] == '2') {
            out[n - 1u] = '\0';
        }
    }
}

static void kbo_cbt_exception_ensure_seed_loaded(void)
{
    if (InterlockedCompareExchange(&g_cbt_season_seed_loaded, 1, 1) == 1) {
        return;
    }

    kbo_cbt_exception_lock(&g_cbt_season_seed_lock);
    if (g_cbt_season_seed_loaded) {
        kbo_cbt_exception_unlock(&g_cbt_season_seed_lock);
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_exception_seed_path(path, sizeof(path))) {
        InterlockedExchange(&g_cbt_season_seed_loaded, 1);
        kbo_cbt_exception_unlock(&g_cbt_season_seed_lock);
        return;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        append_logf("KBO CBT exception seed unavailable path=%s", path);
        if (kbo_cbt_exception_bundled_seed_path(path, sizeof(path))) {
            reader = kbo_csv_reader_open(path);
        }
        if (reader == NULL) {
            append_logf("KBO CBT exception bundled seed unavailable path=%s", path);
            InterlockedExchange(&g_cbt_season_seed_loaded, 1);
            kbo_cbt_exception_unlock(&g_cbt_season_seed_lock);
            return;
        }
    }

    while (g_cbt_season_seed_count < KBO_CBT_EXCEPTION_SEASON_SEED_MAX
            && kbo_csv_reader_next_row(reader)) {
        char fields[5][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 5);
        if (field_count < 5
                || fields[0][0] == '\0'
                || fields[0][0] == '#'
                || _stricmp(fields[0], "player_key") == 0) {
            continue;
        }

        KboCbtSeasonSeedRow row;
        memset(&row, 0, sizeof(row));
        snprintf(row.player_key, sizeof(row.player_key), "%.*s", (int)sizeof(row.player_key) - 1, fields[0]);
        snprintf(row.team_code, sizeof(row.team_code), "%.*s", (int)sizeof(row.team_code) - 1, fields[3]);
        row.season_count = (int)strtol(fields[4], NULL, 10);
        if (row.player_key[0] != '\0' && row.team_code[0] != '\0') {
            g_cbt_season_seed[g_cbt_season_seed_count++] = row;
        }
    }

    kbo_csv_reader_close(reader);
    append_logf("KBO CBT exception seed loaded rows=%d path=%s", g_cbt_season_seed_count, path);
    InterlockedExchange(&g_cbt_season_seed_loaded, 1);
    kbo_cbt_exception_unlock(&g_cbt_season_seed_lock);
}

int kbo_cbt_exception_player_eligible(uint32_t team_id, const char* player_key, int* out_season_count)
{
    if (out_season_count != NULL) {
        *out_season_count = 0;
    }
    if (team_id == 0u || player_key == NULL || player_key[0] == '\0') {
        return 0;
    }
    char team_code[16] = {0};
    kbo_cbt_exception_copy_team_seed_code(team_id, team_code, sizeof(team_code));
    if (team_code[0] == '\0') {
        return 0;
    }

    kbo_cbt_exception_ensure_seed_loaded();
    for (int i = 0; i < g_cbt_season_seed_count; i++) {
        if (_stricmp(g_cbt_season_seed[i].player_key, player_key) == 0
                && _stricmp(g_cbt_season_seed[i].team_code, team_code) == 0) {
            if (out_season_count != NULL) {
                *out_season_count = g_cbt_season_seed[i].season_count;
            }
            return g_cbt_season_seed[i].season_count >= 7;
        }
    }
    return 0;
}
