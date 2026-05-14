#include "../../internal/captain_selection_internal.h"

#include "captain_selection_csv_parse.h"

static int kbo_captain_loaded_csv_parse_u32(const char* text, uint32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }

    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint64_t value = 0u;
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10u + (uint64_t)(*p - '0');
        if (value > 0xffffffffu) {
            return 0;
        }
    }

    *out = (uint32_t)value;
    return 1;
}

static int kbo_captain_loaded_csv_parse_i32(const char* text, int32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }

    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    int negative = 0;
    if (*p == '-') {
        negative = 1;
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    int64_t value = 0;
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10 + (int64_t)(*p - '0');
        if ((!negative && value > 2147483647)
                || (negative && value > 2147483648LL)) {
            return 0;
        }
    }

    *out = negative ? (int32_t)-value : (int32_t)value;
    return 1;
}

static void kbo_captain_copy_loaded_csv_text(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    const char* src = value != NULL ? value : "";
    size_t len = strlen(src);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, src, len);
    out[len] = '\0';
}

int kbo_captain_parse_selection_csv_fields(
    char fields[][KBO_CAPTAIN_SELECTION_CSV_FIELD_SIZE],
    int count,
    uint32_t expected_season,
    KboCaptainSelectionRow* out)
{
    if (fields == NULL || out == NULL) {
        return 0;
    }

    if (count < 8 || fields[0][0] == '\0' || _stricmp(fields[0], "date") == 0) {
        return 0;
    }

    KboCaptainSelectionRow row;
    memset(&row, 0, sizeof(row));
    uint32_t parsed_u32 = 0u;
    int32_t parsed_i32 = 0;

    if (kbo_captain_loaded_csv_parse_u32(fields[0], &parsed_u32)) { row.date = parsed_u32; }
    if (kbo_captain_loaded_csv_parse_u32(fields[1], &parsed_u32)) { row.season = parsed_u32; }
    if (row.season != expected_season) {
        return 0;
    }
    if (kbo_captain_loaded_csv_parse_u32(fields[3], &parsed_u32)) { row.league_id = parsed_u32; }
    if (kbo_captain_loaded_csv_parse_u32(fields[4], &parsed_u32)) { row.team_id = parsed_u32; }
    if (row.team_id == 0u) {
        return 0;
    }

    kbo_captain_copy_loaded_csv_text(row.team_name, sizeof(row.team_name), fields[5]);
    if (kbo_captain_loaded_csv_parse_u32(fields[6], &parsed_u32)) { row.player_id = parsed_u32; }
    kbo_captain_copy_loaded_csv_text(row.player_name, sizeof(row.player_name), fields[7]);
    if (count > 8 && kbo_captain_loaded_csv_parse_i32(fields[8], &parsed_i32)) { row.score = parsed_i32; }
    if (count > 9) { kbo_captain_copy_loaded_csv_text(row.reason, sizeof(row.reason), fields[9]); }
    if (count > 10 && kbo_captain_loaded_csv_parse_u32(fields[10], &parsed_u32)) { row.seeded = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 11 && kbo_captain_loaded_csv_parse_i32(fields[11], &parsed_i32)) { row.seed_priority = parsed_i32; }
    if (count > 12) { kbo_captain_copy_loaded_csv_text(row.seed_source, sizeof(row.seed_source), fields[12]); }
    if (count > 13 && kbo_captain_loaded_csv_parse_u32(fields[13], &parsed_u32)) { row.nation_id = parsed_u32; }
    if (count > 14 && kbo_captain_loaded_csv_parse_u32(fields[14], &parsed_u32)) { row.domestic = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 15 && kbo_captain_loaded_csv_parse_u32(fields[15], &parsed_u32)) { row.current_team_id = parsed_u32; }
    if (count > 16 && kbo_captain_loaded_csv_parse_u32(fields[16], &parsed_u32)) { row.active_team_id = parsed_u32; }
    if (count > 17 && kbo_captain_loaded_csv_parse_u32(fields[17], &parsed_u32)) { row.current_league_id = parsed_u32; }
    if (count > 18 && kbo_captain_loaded_csv_parse_u32(fields[18], &parsed_u32)) { row.age = (uint16_t)parsed_u32; }
    if (count > 19 && kbo_captain_loaded_csv_parse_i32(fields[19], &parsed_i32)) { row.salary = parsed_i32; }
    if (count > 20 && kbo_captain_loaded_csv_parse_i32(fields[20], &parsed_i32)) { row.value_score = parsed_i32; }
    if (count > 21 && kbo_captain_loaded_csv_parse_i32(fields[21], &parsed_i32)) { row.overall_value = (int16_t)parsed_i32; }
    if (count > 22 && kbo_captain_loaded_csv_parse_i32(fields[22], &parsed_i32)) { row.talent_value = (int16_t)parsed_i32; }
    if (count > 23 && kbo_captain_loaded_csv_parse_i32(fields[23], &parsed_i32)) { row.ratings_value = (int16_t)parsed_i32; }
    if (count > 24 && kbo_captain_loaded_csv_parse_i32(fields[24], &parsed_i32)) { row.career_value = (int16_t)parsed_i32; }
    if (count > 25 && kbo_captain_loaded_csv_parse_u32(fields[25], &parsed_u32)) { row.dfa = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 26 && kbo_captain_loaded_csv_parse_u32(fields[26], &parsed_u32)) { row.restricted = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 27 && kbo_captain_loaded_csv_parse_u32(fields[27], &parsed_u32)) { row.injured = parsed_u32 != 0u ? 1u : 0u; }

    *out = row;
    return 1;
}
