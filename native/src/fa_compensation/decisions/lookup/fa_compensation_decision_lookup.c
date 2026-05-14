#include "../fa_compensation_decisions_internal.h"
#include "../../../core/csv/core_csv.h"

int kbo_load_latest_fa_compensation_decision(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (fa_player_id == 0u || out == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
    }
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int found = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[16][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 16);
        if (field_count < 16 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }
        if (kbo_fa_compensation_parse_u32(fields[0]) == fa_player_id) {
            out->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
            out->season = kbo_fa_compensation_parse_u32(fields[1]);
            kbo_fa_compensation_copy_token(fields[2], out->grade, sizeof(out->grade));
            out->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
            out->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
            out->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
            out->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
            out->decided_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[7]);
            kbo_fa_compensation_copy_token(fields[8], out->action, sizeof(out->action));
            out->selected_player_id = kbo_fa_compensation_parse_u32(fields[9]);
            kbo_fa_compensation_copy_token(fields[10], out->selected_player_name, sizeof(out->selected_player_name));
            out->selected_player_score = kbo_fa_compensation_parse_i32(fields[11]);
            out->unprotected_candidate_count = (int)kbo_fa_compensation_parse_u32(fields[12]);
            out->cash_with_player = kbo_fa_compensation_parse_u32(fields[13]);
            out->cash_only = kbo_fa_compensation_parse_u32(fields[14]);
            kbo_fa_compensation_copy_token(fields[15], out->source, sizeof(out->source));
            found = out->selected_player_id != 0u || strcmp(out->action, "CASH_ONLY") == 0;
        }
    }

    kbo_csv_reader_close(reader);
    return found;
}

