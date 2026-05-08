#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "fa_market_classification.h"
#include "fa_market_seed_cases.h"

static int kbo_get_fa_market_cases_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_market_cases_seed.csv", out, out_size);
}

static void kbo_fa_market_copy_token(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
        value++;
    }

    size_t used = 0;
    for (const char* p = value; *p != '\0' && used + 1u < out_size; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\r' || ch == '\n') {
            break;
        }
        out[used++] = (char)ch;
    }
    while (used > 0u && (out[used - 1u] == ' ' || out[used - 1u] == '\t')) {
        used--;
    }
    out[used] = '\0';
}

static int kbo_fa_market_parse_csv_field(char** cursor, char* out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }

    size_t used = 0;
    if (*p == '"') {
        p++;
        while (*p != '\0') {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (used + 1u < out_size) {
                        out[used++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                break;
            }
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (*p != '\0' && *p != ',' && *p != '\n') {
            p++;
        }
    } else {
        while (*p != '\0' && *p != ',' && *p != '\n') {
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (used > 0u && (out[used - 1u] == ' ' || out[used - 1u] == '\t' || out[used - 1u] == '\r')) {
            used--;
        }
    }

    out[used] = '\0';
    if (*p == ',') {
        p++;
    }
    *cursor = p;
    return 1;
}

static int kbo_fa_market_seed_case_allowed(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_DEFERRED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_REQUALIFICATION_LOCKED") == 0
        || strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0
        || strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0
        || strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0
        || strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0
        || strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0
        || strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0
        || strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0
        || strcmp(case_label, "FOREIGN_FREE") == 0;
}

const char* kbo_fa_market_canonical_case_label(const char* case_label)
{
    if (case_label != NULL && strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) {
        return "DOMESTIC_TEAMLESS_UNVERIFIED";
    }
    return case_label;
}

int kbo_fa_market_case_is_seeded_official(const char* case_label)
{
    return case_label != NULL
        && (strcmp(case_label, "KBO_FA_APPROVED") == 0
            || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
            || strcmp(case_label, "KBO_FA_DEFERRED") == 0);
}

static void kbo_ensure_fa_market_cases_seed_template(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_market_cases_seed_path(path, sizeof(path))) {
        return;
    }

    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const char* header =
        "player_id,kbo_case,kbo_grade,note\r\n"
        "# Optional manual truth table for official KBO FA statuses, releases, and domestic teamless cases.\r\n"
        "# kbo_case: KBO_FA_APPROVED,KBO_FA_ELIGIBLE_NOT_APPROVED,KBO_FA_DEFERRED,KBO_FA_BY_HISTORY_UNGRADED,DOMESTIC_RELEASED_NON_FA,DOMESTIC_UNDRAFTED_FREE_AGENT,DOMESTIC_INDEPENDENT_LEAGUE_FA,DOMESTIC_TEAMLESS_UNVERIFIED\r\n"
        "# kbo_grade: A,B,C,UNKNOWN\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    CloseHandle(file);
    append_logf("FA market seed template created path=%s", path);
}

int kbo_load_fa_market_seed_cases(KboFaMarketSeedCase* seeds, int max_seeds, char* out_path, size_t out_path_size)
{
    if (seeds == NULL || max_seeds <= 0) {
        return 0;
    }
    memset(seeds, 0, (SIZE_T)max_seeds * sizeof(seeds[0]));
    kbo_ensure_fa_market_cases_seed_template();

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_market_cases_seed_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
    if (!ReadFile(file, buffer, size, &read, NULL)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    buffer[read] = '\0';

    int count = 0;
    char* cursor = buffer;
    while (*cursor != '\0' && count < max_seeds) {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }

        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p != '\0' && *p != '#' && *p != ';' && (*p < 'A' || *p > 'z')) {
            char player_field[32] = {0};
            char case_field[48] = {0};
            char grade_field[12] = {0};
            char note_field[128] = {0};
            if (kbo_fa_market_parse_csv_field(&p, player_field, sizeof(player_field))
                    && kbo_fa_market_parse_csv_field(&p, case_field, sizeof(case_field))
                    && kbo_fa_market_parse_csv_field(&p, grade_field, sizeof(grade_field))) {
                kbo_fa_market_parse_csv_field(&p, note_field, sizeof(note_field));

                char* tail = NULL;
                unsigned long player_id = strtoul(player_field, &tail, 10);
                if (tail != player_field
                        && player_id > 0ul
                        && player_id <= 0xfffffffful
                        && kbo_fa_market_seed_case_allowed(case_field)) {
                    seeds[count].player_id = (uint32_t)player_id;
                    kbo_fa_market_copy_token(case_field, seeds[count].case_label, sizeof(seeds[count].case_label));
                    kbo_fa_market_copy_token(grade_field[0] != '\0' ? grade_field : "UNKNOWN", seeds[count].grade, sizeof(seeds[count].grade));
                    kbo_fa_market_copy_token(note_field, seeds[count].note, sizeof(seeds[count].note));
                    count++;
                }
            }
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

const KboFaMarketSeedCase* kbo_find_fa_market_seed_case(
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    uint32_t player_id)
{
    if (seeds == NULL || seed_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < seed_count; i++) {
        if (seeds[i].player_id == player_id) {
            return &seeds[i];
        }
    }
    return NULL;
}
