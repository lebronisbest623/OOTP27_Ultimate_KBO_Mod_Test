#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../core/csv/core_csv.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../state/text/state_text_utils.h"
#include "ui_uniform_numbers.h"

typedef struct KboHubUniformNumberEntry {
    uint32_t player_id;
    char number[8];
} KboHubUniformNumberEntry;

#define KBO_HUB_UNIFORM_NUMBER_CACHE_CAP 8192

static KboHubUniformNumberEntry g_kbo_hub_uniform_number_cache[KBO_HUB_UNIFORM_NUMBER_CACHE_CAP];
static int g_kbo_hub_uniform_number_cache_count = 0;
static char g_kbo_hub_uniform_number_cache_save_path[MAX_PATH] = {0};

static void kbo_hub_uniform_number_cache_put(uint32_t player_id, const char* number)
{
    if (player_id == 0u || number == NULL || number[0] == '\0') {
        return;
    }

    char clean[8] = {0};
    snprintf(clean, sizeof(clean), "%s", number);
    kbo_hub_trim_ascii(clean);
    if (clean[0] == '\0') {
        return;
    }

    for (int i = 0; i < g_kbo_hub_uniform_number_cache_count; i++) {
        if (g_kbo_hub_uniform_number_cache[i].player_id == player_id) {
            snprintf(g_kbo_hub_uniform_number_cache[i].number, sizeof(g_kbo_hub_uniform_number_cache[i].number), "%s", clean);
            return;
        }
    }
    if (g_kbo_hub_uniform_number_cache_count >= KBO_HUB_UNIFORM_NUMBER_CACHE_CAP) {
        return;
    }

    KboHubUniformNumberEntry* entry = &g_kbo_hub_uniform_number_cache[g_kbo_hub_uniform_number_cache_count++];
    entry->player_id = player_id;
    snprintf(entry->number, sizeof(entry->number), "%s", clean);
}

static void kbo_hub_load_uniform_numbers_from_csv(const char* path, int id_column, int number_column)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return;
    }

    int max_column = id_column > number_column ? id_column : number_column;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[16][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 16);
        if (field_count <= max_column
                || fields[0][0] == '\0'
                || fields[0][0] == '#'
                || fields[0][0] == '/'
                || fields[0][0] == ';') {
            continue;
        }

        const char* id_text = fields[id_column];
        const char* number_text = fields[number_column];
        if (id_text[0] < '0' || id_text[0] > '9') {
            continue;
        }
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        kbo_hub_uniform_number_cache_put(player_id, number_text);
    }

    kbo_csv_reader_close(reader);
}

static void kbo_hub_load_uniform_numbers_for_current_save(void)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        g_kbo_hub_uniform_number_cache_count = 0;
        g_kbo_hub_uniform_number_cache_save_path[0] = '\0';
        return;
    }
    if (_stricmp(g_kbo_hub_uniform_number_cache_save_path, save_path) == 0) {
        return;
    }

    g_kbo_hub_uniform_number_cache_count = 0;
    snprintf(g_kbo_hub_uniform_number_cache_save_path, sizeof(g_kbo_hub_uniform_number_cache_save_path), "%s", save_path);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\import_export\\kbo_rosters.csv", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 8);
    snprintf(path, sizeof(path), "%s\\import_export\\kbo_rosters.txt", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 8);
    snprintf(path, sizeof(path), "%s\\data\\derived\\matching\\uniform_number_patch_report.csv", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 7);
}

void kbo_webview_copy_player_uniform_number(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (player_id == 0u) {
        return;
    }

    kbo_hub_load_uniform_numbers_for_current_save();
    for (int i = 0; i < g_kbo_hub_uniform_number_cache_count; i++) {
        if (g_kbo_hub_uniform_number_cache[i].player_id == player_id) {
            snprintf(out, out_size, "%s", g_kbo_hub_uniform_number_cache[i].number);
            return;
        }
    }
}
