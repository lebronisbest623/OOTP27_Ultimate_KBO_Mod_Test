#include "military_service_team_policy_parse.h"

#include <string.h>

#include "../../../core/csv/core_csv.h"

static int kbo_military_service_team_policy_enabled_value(const char* text, int* out_enabled)
{
    if (out_enabled != 0) {
        *out_enabled = 0;
    }
    if (text == 0 || text[0] == '\0' || out_enabled == 0) {
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

int kbo_parse_military_service_team_policy_line(
    const char* line,
    KboMilitaryServiceTeamPolicyRow* out)
{
    if (out != 0) {
        memset(out, 0, sizeof(*out));
    }
    if (line == 0 || out == 0) {
        return 0;
    }

    char fields[3][32];
    int field_count = kbo_csv_read_trimmed_line_fields(
        line,
        (char*)fields,
        sizeof(fields[0]),
        3);
    if (field_count <= 0
            || fields[0][0] == '\0'
            || fields[0][0] == '#'
            || fields[0][0] == ';'
            || _stricmp(fields[0], "team_csv_id") == 0
            || _stricmp(fields[0], "csv_id") == 0
            || _stricmp(fields[0], "team_id") == 0) {
        return 0;
    }
    if (field_count < 2 || !kbo_military_service_team_policy_enabled_value(fields[1], &out->enabled)) {
        return 0;
    }

    size_t team_id_len = strlen(fields[0]);
    if (team_id_len == 0u || team_id_len >= sizeof(out->team_csv_id)) {
        return 0;
    }
    memcpy(out->team_csv_id, fields[0], team_id_len + 1u);
    return out->team_csv_id[0] != '\0';
}
