#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbt_records.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../fa_salary_snapshot/csv/salary_snapshot_csv_parse.h"

static int kbo_cbt_records_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("cbt_records.csv", out, out_size);
}

static void kbo_cbt_parse_record_line(const char* line, KboCbtRecord* rec)
{
    memset(rec, 0, sizeof(*rec));
    char* p = (char*)line;

    char field[128] = {0};
    int fi = 0;
    while (fi <= 9 && *p != '\0') {
        if (!kbo_fa_salary_snapshot_parse_csv_field(&p, field, sizeof(field))) {
            break;
        }
        switch (fi) {
        case 0: rec->season            = (uint32_t)strtoul(field, NULL, 10); break;
        case 1: rec->team_id           = (uint32_t)strtoul(field, NULL, 10); break;
        case 2: rec->payroll           = (int32_t)strtol(field, NULL, 10); break;
        case 3: rec->threshold         = (int32_t)strtol(field, NULL, 10); break;
        case 4: rec->overage           = (int32_t)strtol(field, NULL, 10); break;
        case 5: rec->tax_rate_pct      = (uint32_t)strtoul(field, NULL, 10); break;
        case 6: rec->tax_amount        = (int32_t)strtol(field, NULL, 10); break;
        case 7: rec->consecutive_count = (uint32_t)strtoul(field, NULL, 10); break;
        case 8: rec->processed_date    = (uint32_t)strtoul(field, NULL, 10); break;
        case 9:
            snprintf(rec->team_name, sizeof(rec->team_name), "%s", field);
            break;
        }
        fi++;
    }
}

int kbo_cbt_load_records(KboCbtRecord* records, int max, char* path_out, size_t path_size)
{
    if (records == NULL || max <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max * sizeof(records[0]));
    if (path_out != NULL && path_size > 0) {
        path_out[0] = '\0';
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_records_path(path, sizeof(path))) {
        return 0;
    }
    if (path_out != NULL && path_size > 0) {
        snprintf(path_out, path_size, "%s", path);
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buf == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int count = 0;
    if (ReadFile(file, buf, size, &read, NULL) && read > 0) {
        buf[read] = '\0';
        char* cursor = buf;
        int header_skipped = 0;
        while (*cursor != '\0' && count < max) {
            char* next = strchr(cursor, '\n');
            if (next != NULL) {
                *next = '\0';
            }
            char* p = cursor;
            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
            if (!header_skipped) {
                header_skipped = 1;
            } else if (*p >= '1' && *p <= '9') {
                kbo_cbt_parse_record_line(p, &records[count]);
                if (records[count].season > 0u && records[count].team_id > 0u) {
                    count++;
                }
            }
            if (next == NULL) {
                break;
            }
            cursor = next + 1;
        }
    }

    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buf);
    return count;
}

int kbo_cbt_save_records(const KboCbtRecord* records, int count)
{
    if (records == NULL || count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_records_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("CBT records open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "season,team_id,payroll,threshold,overage,tax_rate_pct,tax_amount,consecutive_count,processed_date,team_name\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < count; i++) {
        const KboCbtRecord* r = &records[i];
        if (r->season == 0u || r->team_id == 0u) {
            continue;
        }
        char line[512] = {0};
        int len = snprintf(
            line, sizeof(line),
            "%u,%u,%d,%d,%d,%u,%d,%u,%u,%s\r\n",
            r->season, r->team_id, r->payroll, r->threshold, r->overage,
            r->tax_rate_pct, r->tax_amount, r->consecutive_count,
            r->processed_date, r->team_name);
        if (len > 0) {
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    CloseHandle(file);
    return 1;
}

uint32_t kbo_cbt_get_consecutive_count(const KboCbtRecord* records, int count,
    uint32_t team_id, uint32_t before_season)
{
    if (records == NULL || count <= 0 || team_id == 0u || before_season == 0u) {
        return 0u;
    }

    /* Walk backwards from before_season counting consecutive prior violations */
    uint32_t streak = 0u;
    for (uint32_t s = before_season - 1u; s >= 1900u; s--) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (records[i].season == s && records[i].team_id == team_id) {
                if (records[i].overage > 0) {
                    streak++;
                    found = 1;
                }
                break;
            }
        }
        if (!found) {
            break;
        }
    }
    return streak;
}

int kbo_cbt_find_record(const KboCbtRecord* records, int count, uint32_t season, uint32_t team_id)
{
    if (records == NULL || count <= 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (records[i].season == season && records[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}
