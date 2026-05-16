#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../fa_declaration.h"
#include "../fa_declaration_internal.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../fa_filing/fa_filing_parts/fa_filing_csv_parse.h"
#include "../../fa_filing/fa_filing_parts/fa_filing_csv_write_helpers.h"

int kbo_load_fa_declaration_report_rows(
    KboFaDeclarationReportRow* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_rows && kbo_csv_reader_next_row(reader)) {
        char fields[23][192];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 23);
        if (field_count < 17 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboFaDeclarationReportRow row;
        memset(&row, 0, sizeof(row));
        row.declaration_date = kbo_fa_filing_parse_u32(fields[0]);
        row.season = kbo_fa_filing_parse_u32(fields[1]);
        row.player_id = kbo_fa_filing_parse_u32(fields[2]);
        kbo_fa_filing_copy_text(row.player_name, sizeof(row.player_name), fields[3]);
        row.declared = kbo_fa_filing_parse_u32(fields[4]);
        row.team_id = kbo_fa_filing_parse_u32(fields[5]);
        row.league_id = kbo_fa_filing_parse_u32(fields[6]);
        row.nation_id = kbo_fa_filing_parse_u32(fields[7]);
        row.age = (uint16_t)(kbo_fa_filing_parse_u32(fields[8]) & 0xffffu);
        row.contract_level = (uint8_t)(kbo_fa_filing_parse_u32(fields[9]) & 0xffu);
        row.salary = (int32_t)kbo_fa_filing_parse_u32(fields[10]);
        row.fa_demand = (int32_t)kbo_fa_filing_parse_u32(fields[11]);
        row.score = (int32_t)kbo_fa_filing_parse_u32(fields[12]);
        row.threshold = (int32_t)kbo_fa_filing_parse_u32(fields[13]);
        kbo_fa_filing_copy_text(row.grade, sizeof(row.grade), fields[14]);
        kbo_fa_filing_copy_text(row.case_label, sizeof(row.case_label), fields[15]);
        row.overall = (int16_t)kbo_fa_filing_parse_u32(fields[16]);
        if (field_count > 17) { row.talent = (int16_t)kbo_fa_filing_parse_u32(fields[17]); }
        if (field_count > 18) { row.ratings = (int16_t)kbo_fa_filing_parse_u32(fields[18]); }
        if (field_count > 19) { row.career = (int16_t)kbo_fa_filing_parse_u32(fields[19]); }
        if (field_count > 20) { kbo_fa_filing_copy_text(row.source, sizeof(row.source), fields[20]); }
        if (field_count > 21) { kbo_fa_filing_copy_text(row.reason, sizeof(row.reason), fields[21]); }
        if (field_count > 22) { kbo_fa_filing_copy_text(row.decision_reason, sizeof(row.decision_reason), fields[22]); }
        if (row.player_id != 0u && row.season != 0u) {
            rows[count++] = row;
        }
    }

    kbo_csv_reader_close(reader);
    return count;
}

