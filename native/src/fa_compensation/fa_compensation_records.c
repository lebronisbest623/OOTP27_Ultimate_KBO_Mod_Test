#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../core/core_atomic_file.h"
#include "../core/core_log.h"
#include "fa_compensation_paths_parse.h"
#include "fa_compensation_records.h"

static int kbo_fa_compensation_parse_csv_field(char** cursor, char* out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    char* p = *cursor;
    size_t used = 0;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }
    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (used + 1 < out_size) {
                        out[used++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                if (*p == ',') {
                    p++;
                }
                break;
            }
        } else if (*p == ',') {
            p++;
            break;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }
        if (used + 1 < out_size) {
            out[used++] = *p;
        }
        p++;
    }
    out[used] = '\0';
    *cursor = p;
    return 1;
}

static void kbo_fa_compensation_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        const char* p = text;
        while (*p != '\0') {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
            p++;
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

int kbo_load_fa_compensation_records(
    KboFaCompensationRecord* records,
    int max_records,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (records == NULL || max_records <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max_records * sizeof(records[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
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
    while (*cursor != '\0' && count < max_records) {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }

        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            char fields[16][128];
            memset(fields, 0, sizeof(fields));
            int ok = 1;
            for (int field_index = 0; field_index < 16; field_index++) {
                if (!kbo_fa_compensation_parse_csv_field(&p, fields[field_index], sizeof(fields[field_index]))) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                KboFaCompensationRecord* rec = &records[count];
                rec->player_id = kbo_fa_compensation_parse_u32(fields[0]);
                rec->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[1]);
                rec->season = kbo_fa_compensation_parse_u32(fields[2]);
                rec->league_id = kbo_fa_compensation_parse_u32(fields[3]);
                rec->original_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                rec->signing_team_id = kbo_fa_compensation_parse_u32(fields[5]);
                kbo_fa_compensation_copy_token(fields[6], rec->grade, sizeof(rec->grade));
                rec->previous_salary = kbo_fa_compensation_parse_i32(fields[7]);
                rec->cash_with_player = kbo_fa_compensation_parse_u32(fields[8]);
                rec->cash_only = kbo_fa_compensation_parse_u32(fields[9]);
                rec->protect_count = kbo_fa_compensation_parse_u32(fields[10]);
                rec->requires_player_compensation = kbo_fa_compensation_parse_u32(fields[11]) != 0u ? 1u : 0u;
                rec->status = (uint8_t)kbo_fa_compensation_parse_u32(fields[12]);
                kbo_fa_compensation_copy_token(fields[13], rec->case_label, sizeof(rec->case_label));
                kbo_fa_compensation_copy_token(fields[14], rec->player_name, sizeof(rec->player_name));
                kbo_fa_compensation_copy_token(fields[15], rec->source, sizeof(rec->source));
                if (rec->player_id != 0u && rec->season >= 1982u && rec->season <= 2300u) {
                    count++;
                } else {
                    memset(rec, 0, sizeof(*rec));
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

int kbo_persist_fa_compensation_records(const KboFaCompensationRecord* records, int record_count)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        append_log_line("KBO FA compensation persist skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation persist failed reason=create_tmp gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "player_id,signed_on,season,league_id,original_team_id,signing_team_id,grade,"
        "previous_salary,cash_with_player,cash_only,protect_count,requires_player_compensation,"
        "status,kbo_case,player_name,source\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == 0u || rec->season == 0u) {
            continue;
        }
        char prefix[192] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,%u,%u,%u,",
            rec->player_id,
            rec->signed_on_yyyymmdd,
            rec->season,
            rec->league_id,
            rec->original_team_id,
            rec->signing_team_id);
        if (len > 0 && len < (int)sizeof(prefix)) {
            WriteFile(file, prefix, (DWORD)len, &written, NULL);
        }
        kbo_fa_compensation_write_csv_text(file, rec->grade);

        char middle[192] = {0};
        len = snprintf(
            middle,
            sizeof(middle),
            ",%d,%u,%u,%u,%u,%u,",
            rec->previous_salary,
            rec->cash_with_player,
            rec->cash_only,
            rec->protect_count,
            (uint32_t)rec->requires_player_compensation,
            (uint32_t)rec->status);
        if (len > 0 && len < (int)sizeof(middle)) {
            WriteFile(file, middle, (DWORD)len, &written, NULL);
        }
        kbo_fa_compensation_write_csv_text(file, rec->case_label);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_compensation_write_csv_text(file, rec->player_name);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_compensation_write_csv_text(file, rec->source);
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    int ok = kbo_atomic_commit(file, tmp_path, path);
    if (!ok) {
        append_logf("KBO FA compensation atomic commit failed path=%s", path);
        return 0;
    }
    append_logf("KBO FA compensation persisted records=%d path=%s", record_count, path);
    return 1;
}

int kbo_append_fa_compensation_record(const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        append_log_line("KBO FA compensation append skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation append failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (needs_header) {
        const char* header =
            "player_id,signed_on,season,league_id,original_team_id,signing_team_id,grade,"
            "previous_salary,cash_with_player,cash_only,protect_count,requires_player_compensation,"
            "status,kbo_case,player_name,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char prefix[192] = {0};
    int len = snprintf(
        prefix,
        sizeof(prefix),
        "%u,%u,%u,%u,%u,%u,",
        rec->player_id,
        rec->signed_on_yyyymmdd,
        rec->season,
        rec->league_id,
        rec->original_team_id,
        rec->signing_team_id);
    if (len > 0 && len < (int)sizeof(prefix)) {
        WriteFile(file, prefix, (DWORD)len, &written, NULL);
    }
    kbo_fa_compensation_write_csv_text(file, rec->grade);

    char middle[192] = {0};
    len = snprintf(
        middle,
        sizeof(middle),
        ",%d,%u,%u,%u,%u,%u,",
        rec->previous_salary,
        rec->cash_with_player,
        rec->cash_only,
        rec->protect_count,
        (uint32_t)rec->requires_player_compensation,
        (uint32_t)rec->status);
    if (len > 0 && len < (int)sizeof(middle)) {
        WriteFile(file, middle, (DWORD)len, &written, NULL);
    }
    kbo_fa_compensation_write_csv_text(file, rec->case_label);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_compensation_write_csv_text(file, rec->player_name);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_compensation_write_csv_text(file, rec->source);
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);
    return 1;
}

int kbo_fa_compensation_find_existing(
    const KboFaCompensationRecord* records,
    int record_count,
    uint32_t player_id,
    uint32_t season,
    uint32_t original_team_id,
    uint32_t signing_team_id)
{
    if (records == NULL || player_id == 0u || season == 0u) {
        return -1;
    }
    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == player_id
                && rec->season == season
                && rec->original_team_id == original_team_id
                && rec->signing_team_id == signing_team_id) {
            return i;
        }
    }
    return -1;
}
