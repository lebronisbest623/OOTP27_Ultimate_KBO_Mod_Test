#include "team_classification_seed_parse.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/csv/core_csv.h"

static int kbo_team_classification_enabled_value(const char* text, int* out_enabled)
{
    if (out_enabled != NULL) {
        *out_enabled = 0;
    }
    if (text == NULL || text[0] == '\0' || out_enabled == NULL) {
        return 0;
    }

    if (_stricmp(text, "1") == 0
            || _stricmp(text, "true") == 0
            || _stricmp(text, "yes") == 0
            || _stricmp(text, "on") == 0
            || _stricmp(text, "enabled") == 0) {
        *out_enabled = 1;
        return 1;
    }
    if (_stricmp(text, "0") == 0
            || _stricmp(text, "false") == 0
            || _stricmp(text, "no") == 0
            || _stricmp(text, "off") == 0
            || _stricmp(text, "disabled") == 0) {
        *out_enabled = 0;
        return 1;
    }
    return 0;
}

static int kbo_team_classification_copy_field(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (value == NULL) {
        return 0;
    }
    size_t len = strlen(value);
    if (len >= out_size) {
        return 0;
    }
    memcpy(out, value, len + 1u);
    return len > 0u;
}

int kbo_parse_team_classification_seed_line(
    const char* line,
    KboTeamClassificationSeedRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (line == NULL || out == NULL) {
        return 0;
    }

    char fields[5][96];
    int field_count = kbo_csv_read_trimmed_line_fields(
        line,
        (char*)fields,
        sizeof(fields[0]),
        5);
    if (field_count <= 0
            || fields[0][0] == '\0'
            || fields[0][0] == '#'
            || fields[0][0] == ';'
            || _stricmp(fields[0], "team_csv_id") == 0
            || _stricmp(fields[0], "csv_id") == 0
            || _stricmp(fields[0], "team_id") == 0) {
        return 0;
    }

    if (field_count < 4
            || !kbo_team_classification_enabled_value(fields[1], &out->enabled)
            || !kbo_team_classification_copy_field(out->team_csv_id, sizeof(out->team_csv_id), fields[0])
            || !kbo_team_classification_copy_field(out->team_type, sizeof(out->team_type), fields[2])
            || !kbo_team_classification_copy_field(out->league_level, sizeof(out->league_level), fields[3])) {
        memset(out, 0, sizeof(*out));
        return 0;
    }

    if (field_count >= 5 && fields[4][0] != '\0') {
        kbo_team_classification_copy_field(out->display_name, sizeof(out->display_name), fields[4]);
    }
    return 1;
}

int kbo_team_classification_seed_row_is_independent_futures(
    const KboTeamClassificationSeedRow* row)
{
    if (row == NULL || !row->enabled || row->team_csv_id[0] == '\0') {
        return 0;
    }
    if (_stricmp(row->team_type, "independent") != 0) {
        return 0;
    }
    return _stricmp(row->league_level, "futures") == 0
        || _stricmp(row->league_level, "minor") == 0
        || _stricmp(row->league_level, "farm") == 0;
}
