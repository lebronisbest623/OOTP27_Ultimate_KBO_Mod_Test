#include "captain_seed_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/csv/core_csv.h"

int kbo_captain_parse_u32_full_token(const char* text, uint32_t* out)
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

int kbo_captain_parse_i32_full_token(const char* text, int32_t* out)
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

static int kbo_captain_parse_status_token(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 1;
    }
    if (text[0] == '1') {
        return 1;
    }
    if (text[0] == '0') {
        return 0;
    }
    if (_stricmp(text, "active") == 0 || _stricmp(text, "enabled") == 0
            || _stricmp(text, "yes") == 0 || _stricmp(text, "true") == 0) {
        return 1;
    }
    if (_stricmp(text, "inactive") == 0 || _stricmp(text, "disabled") == 0
            || _stricmp(text, "no") == 0 || _stricmp(text, "false") == 0) {
        return 0;
    }
    return 1;
}

static void kbo_captain_copy_seed_string(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s", value != NULL ? value : "");
}

static void kbo_captain_parse_player_token(const char* text, uint32_t* out_player_id, char* out_key, size_t out_key_size)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (out_key != NULL && out_key_size > 0) {
        out_key[0] = '\0';
    }
    if (text == NULL || text[0] == '\0') {
        return;
    }

    uint32_t parsed = 0u;
    if (kbo_captain_parse_u32_full_token(text, &parsed)) {
        if (out_player_id != NULL) {
            *out_player_id = parsed;
        }
        return;
    }

    if (out_key != NULL && out_key_size > 0) {
        snprintf(out_key, out_key_size, "%s", text);
    }
}

int kbo_parse_captain_seed_fields(char fields[][128], int field_count, KboCaptainSeed* out)
{
    if (fields == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->priority = 100;
    out->active = 1u;

    if (field_count <= 0 || fields[0][0] == '\0'
            || fields[0][0] == '#'
            || fields[0][0] == ';'
            || _stricmp(fields[0], "season") == 0
            || _stricmp(fields[0], "team_id") == 0
            || _stricmp(fields[0], "team_code") == 0) {
        return 0;
    }

    uint32_t parsed_u32 = 0u;
    int32_t parsed_i32 = 0;

    if (field_count >= 6) {
        if (fields[0][0] != '\0' && kbo_captain_parse_u32_full_token(fields[0], &parsed_u32)) {
            out->season = parsed_u32;
        }
        if (fields[1][0] != '\0' && kbo_captain_parse_u32_full_token(fields[1], &parsed_u32)) {
            out->league_id = parsed_u32;
        }
        if (fields[2][0] != '\0' && kbo_captain_parse_u32_full_token(fields[2], &parsed_u32)) {
            out->team_id = parsed_u32;
        }
        kbo_captain_copy_seed_string(out->team_code, sizeof(out->team_code), fields[3]);
        kbo_captain_parse_player_token(fields[4], &out->player_id, out->player_key, sizeof(out->player_key));
        if (fields[5][0] != '\0') {
            kbo_captain_copy_seed_string(out->player_key, sizeof(out->player_key), fields[5]);
        }
        if (field_count > 6) {
            kbo_captain_copy_seed_string(out->player_name, sizeof(out->player_name), fields[6]);
        }
        if (field_count > 7 && fields[7][0] != '\0'
                && kbo_captain_parse_i32_full_token(fields[7], &parsed_i32)) {
            out->priority = parsed_i32;
        }
        if (field_count > 8) {
            out->active = kbo_captain_parse_status_token(fields[8]) ? 1u : 0u;
        }
    } else {
        if (kbo_captain_parse_u32_full_token(fields[0], &parsed_u32)) {
            out->team_id = parsed_u32;
        } else {
            kbo_captain_copy_seed_string(out->team_code, sizeof(out->team_code), fields[0]);
        }
        if (field_count > 1) {
            kbo_captain_parse_player_token(fields[1], &out->player_id, out->player_key, sizeof(out->player_key));
        }
        if (field_count > 2) {
            kbo_captain_copy_seed_string(out->player_name, sizeof(out->player_name), fields[2]);
        }
        if (field_count > 3 && fields[3][0] != '\0'
                && kbo_captain_parse_i32_full_token(fields[3], &parsed_i32)) {
            out->priority = parsed_i32;
        }
        if (field_count > 4) {
            out->active = kbo_captain_parse_status_token(fields[4]) ? 1u : 0u;
        }
    }

    return (out->team_id != 0u || out->team_code[0] != '\0')
        && (out->player_id != 0u || out->player_key[0] != '\0');
}

int kbo_parse_captain_seed_line(const char* line, KboCaptainSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }

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

    char fields[10][128];
    int field_count = kbo_csv_read_trimmed_line_fields(p, (char*)fields, sizeof(fields[0]), 10);
    return kbo_parse_captain_seed_fields(fields, field_count, out);
}
