#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbt_records.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../fa_salary_snapshot/csv/salary_snapshot_csv_parse.h"

static int kbo_cbt_records_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("cbt_records.csv", out, out_size);
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

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max && kbo_csv_reader_next_row(reader)) {
        char fields[10][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);
        if (field_count < 10 || fields[0][0] < '1' || fields[0][0] > '9') {
            continue;
        }

        KboCbtRecord* rec = &records[count];
        memset(rec, 0, sizeof(*rec));
        rec->season            = (uint32_t)strtoul(fields[0], NULL, 10);
        rec->team_id           = (uint32_t)strtoul(fields[1], NULL, 10);
        rec->payroll           = (int32_t)strtol(fields[2], NULL, 10);
        rec->threshold         = (int32_t)strtol(fields[3], NULL, 10);
        rec->overage           = (int32_t)strtol(fields[4], NULL, 10);
        rec->tax_rate_pct      = (uint32_t)strtoul(fields[5], NULL, 10);
        rec->tax_amount        = (int32_t)strtol(fields[6], NULL, 10);
        rec->consecutive_count = (uint32_t)strtoul(fields[7], NULL, 10);
        rec->processed_date    = (uint32_t)strtoul(fields[8], NULL, 10);
        snprintf(rec->team_name, sizeof(rec->team_name), "%.*s", (int)sizeof(rec->team_name) - 1, fields[9]);
        if (rec->season > 0u && rec->team_id > 0u) {
            count++;
        }
    }

    kbo_csv_reader_close(reader);
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

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("CBT records open failed path=%s gle=%lu", path, GetLastError());
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

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("CBT records atomic commit failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }
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
