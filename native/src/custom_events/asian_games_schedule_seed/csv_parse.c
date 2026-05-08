#include "../custom_events_common.h"
#include "csv_parse.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../core/core_current_date.h"
#include "../../core/core_save_paths.h"
#include "../../core/core_text_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../foreign/foreign_waiver_date.h"

void kbo_asian_games_schedule_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }

    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1u);
    }

    size_t len = strlen(value);
    while (len > 0u) {
        char c = value[len - 1u];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '"') {
            break;
        }
        value[--len] = '\0';
    }
}

void kbo_asian_games_schedule_read_cell(const char** cursor, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (cursor == NULL || *cursor == NULL) {
        return;
    }

    const char* p = *cursor;
    size_t len = 0u;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }

    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (len + 1u < out_size) {
                        out[len++] = '"';
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

        if (len + 1u < out_size) {
            out[len++] = *p;
        }
        p++;
    }

    out[len] = '\0';
    kbo_asian_games_schedule_trim_cell(out);
    *cursor = p;
}

void kbo_asian_games_schedule_copy_text(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, value, len);
    out[len] = '\0';
}

uint32_t kbo_asian_games_schedule_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    unsigned long long value = 0ull;
    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0u;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10ull + (unsigned long long)(*p - '0');
        if (value > UINT32_MAX) {
            return 0u;
        }
        p++;
    }
    return (uint32_t)value;
}

uint32_t kbo_asian_games_schedule_parse_date(const char* text)
{
    if (text == NULL || text[0] == '\0'
            || ascii_equals_ignore_case(text, "TBD")
            || ascii_equals_ignore_case(text, "NA")
            || ascii_equals_ignore_case(text, "N/A")) {
        return 0u;
    }

    uint32_t date = 0u;
    if (!kbo_parse_yyyymmdd(text, &date)) {
        return 0u;
    }
    if (kbo_date_serial(date / 10000u, (date / 100u) % 100u, date % 100u) == 0u) {
        return 0u;
    }
    return date;
}

uint8_t kbo_asian_games_schedule_parse_auto(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    return (uint8_t)(ascii_equals_ignore_case(text, "1")
        || ascii_equals_ignore_case(text, "true")
        || ascii_equals_ignore_case(text, "yes")
        || ascii_equals_ignore_case(text, "auto")
        || ascii_equals_ignore_case(text, "schedule"));
}

int kbo_parse_asian_games_schedule_seed_line(const char* line, KboAsianGamesScheduleSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[512] = {0};
    size_t len = strlen(line);
    if (len >= sizeof(copy)) {
        len = sizeof(copy) - 1u;
    }
    memcpy(copy, line, len);

    char* p = copy;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    const char* cursor = p;
    char fields[11][128];
    memset(fields, 0, sizeof(fields));
    for (int i = 0; i < 11; i++) {
        kbo_asian_games_schedule_read_cell(&cursor, fields[i], sizeof(fields[i]));
    }

    if (fields[0][0] == '\0'
            || ascii_equals_ignore_case(fields[0], "year")
            || ascii_equals_ignore_case(fields[0], "event_year")) {
        return 0;
    }

    out->year = kbo_asian_games_schedule_parse_u32(fields[0]);
    if (out->year < 1982u || out->year > 2200u) {
        return 0;
    }
    kbo_asian_games_schedule_copy_text(out->host_city, sizeof(out->host_city), fields[1]);
    kbo_asian_games_schedule_copy_text(out->host_country, sizeof(out->host_country), fields[2]);
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status), fields[3][0] != '\0' ? fields[3] : "official");
    out->tournament_start = kbo_asian_games_schedule_parse_date(fields[4]);
    out->tournament_end = kbo_asian_games_schedule_parse_date(fields[5]);
    out->selection_date = kbo_asian_games_schedule_parse_date(fields[6]);
    out->departure_date = kbo_asian_games_schedule_parse_date(fields[7]);
    out->final_date = kbo_asian_games_schedule_parse_date(fields[8]);
    out->auto_schedule = kbo_asian_games_schedule_parse_auto(fields[9]);
    kbo_asian_games_schedule_copy_text(out->notes, sizeof(out->notes), fields[10]);
    return 1;
}
