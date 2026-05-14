#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/csv/core_csv.h"
#include "../../../core/dates/core_text_date.h"
#include "../../calendar/military_service_date.h"
#include "military_service_seed_parse.h"

int kbo_military_ascii_is_seed_id_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

int kbo_military_parse_u32_full_token(const char* text, uint32_t* out)
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

uint32_t kbo_military_parse_yyyymmdd(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }

    char digits[9] = {0};
    size_t count = 0;
    for (const char* p = text; *p != '\0' && count < 8u; p++) {
        if (*p >= '0' && *p <= '9') {
            digits[count++] = *p;
        }
    }
    if (count != 8u) {
        return 0u;
    }
    uint32_t value = (uint32_t)strtoul(digits, NULL, 10);
    uint32_t year = value / 10000u;
    uint32_t month = (value / 100u) % 100u;
    uint32_t day = value % 100u;
    return kbo_date_serial(year, month, day) != 0u ? value : 0u;
}

uint32_t kbo_military_yyyymmdd_to_serial(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return 0u;
    }
    return kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
}

uint32_t kbo_military_days_in_month(uint32_t year, uint32_t month)
{
    static const uint8_t days_by_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1u || month > 12u) {
        return 0u;
    }
    if (month == 2u && kbo_is_leap_year(year)) {
        return 29u;
    }
    return days_by_month[month - 1u];
}

uint32_t kbo_military_serial_to_yyyymmdd(uint32_t serial)
{
    if (serial == 0u) {
        return 0u;
    }

    uint32_t year = serial / 366u + 1u;
    while (kbo_date_serial(year + 1u, 1u, 1u) <= serial) {
        year++;
    }
    while (year > 1u && kbo_date_serial(year, 1u, 1u) > serial) {
        year--;
    }

    uint32_t month = 1u;
    while (month < 12u) {
        uint32_t next_month_serial = month == 12u
            ? kbo_date_serial(year + 1u, 1u, 1u)
            : kbo_date_serial(year, month + 1u, 1u);
        if (next_month_serial > serial) {
            break;
        }
        month++;
    }

    uint32_t month_start = kbo_date_serial(year, month, 1u);
    uint32_t day = serial >= month_start ? (serial - month_start + 1u) : 1u;
    uint32_t month_days = kbo_military_days_in_month(year, month);
    if (month_days != 0u && day > month_days) {
        day = month_days;
    }
    return year * 10000u + month * 100u + day;
}

uint32_t kbo_military_yyyymmdd_add_days(uint32_t yyyymmdd, int32_t days)
{
    uint32_t serial = kbo_military_yyyymmdd_to_serial(yyyymmdd);
    if (serial == 0u || days < 0) {
        return 0u;
    }
    return kbo_military_serial_to_yyyymmdd(serial + (uint32_t)days);
}

int32_t kbo_military_days_left_from_return_serial(uint32_t return_serial, uint32_t today_serial)
{
    if (return_serial == 0u || today_serial == 0u || return_serial <= today_serial) {
        return 0;
    }
    uint32_t diff = return_serial - today_serial;
    return diff > 32767u ? 32767 : (int32_t)diff;
}

int kbo_parse_military_service_seed_fields(char fields[][96], int field_count, KboMilitaryServiceSeed* out)
{
    if (fields == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    if (field_count <= 0 || fields[0][0] == '\0'
            || fields[0][0] == '#'
            || fields[0][0] == ';'
            || _stricmp(fields[0], "source_key") == 0
            || _stricmp(fields[0], "player_id") == 0) {
        return 0;
    }

    snprintf(out->key, sizeof(out->key), "%s", fields[0]);
    uint32_t parsed_u32 = 0u;
    if (kbo_military_parse_u32_full_token(out->key, &parsed_u32)) {
        out->player_id = parsed_u32;
    }

    if (field_count == 3
            && fields[1][0] >= '0' && fields[1][0] <= '9'
            && fields[2][0] >= '0' && fields[2][0] <= '9') {
        uint32_t today_serial = kbo_current_date_serial();
        uint32_t days_left = (uint32_t)strtoul(fields[2], NULL, 10);
        snprintf(out->service_team_code, sizeof(out->service_team_code), "SANG");
        snprintf(out->original_team_code, sizeof(out->original_team_code), "%s", fields[1]);
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        out->service_return_yyyymmdd = today_serial != 0u
            ? kbo_military_serial_to_yyyymmdd(today_serial + days_left)
            : 0u;
        return out->key[0] != '\0' || out->player_id != 0u;
    }

    if (field_count > 5 && fields[5][0] != '\0'
            && kbo_military_parse_u32_full_token(fields[5], &parsed_u32)) {
        out->player_id = parsed_u32;
    }
    snprintf(out->service_team_code, sizeof(out->service_team_code), "%s",
        (field_count > 1 && fields[1][0] != '\0') ? fields[1] : "SANG");
    if (field_count > 2 && fields[2][0] != '\0') {
        snprintf(out->original_team_code, sizeof(out->original_team_code), "%s", fields[2]);
    }

    uint32_t numeric_field4 = 0u;
    int field4_is_numeric = field_count > 4
        && fields[4][0] != '\0'
        && kbo_military_parse_u32_full_token(fields[4], &numeric_field4);
    int looks_like_old_start_total = field_count > 5
        && field4_is_numeric
        && kbo_military_parse_yyyymmdd(fields[3]) != 0u;

    if (looks_like_old_start_total) {
        out->service_start_yyyymmdd = kbo_military_parse_yyyymmdd(fields[3]);
        out->service_total_days = (int32_t)numeric_field4;
        out->service_return_yyyymmdd = kbo_military_yyyymmdd_add_days(
            out->service_start_yyyymmdd,
            out->service_total_days > 0 ? out->service_total_days : KBO_MILITARY_SERVICE_DAYS);
    } else {
        out->service_return_yyyymmdd = field_count > 3 ? kbo_military_parse_yyyymmdd(fields[3]) : 0u;
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        if (field_count > 4 && field4_is_numeric) {
            out->player_id = numeric_field4;
        }
    }
    if (out->service_total_days <= 0) {
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
    }
    return out->key[0] != '\0' || out->player_id != 0u;
}

int kbo_parse_military_service_seed_line(const char* line, KboMilitaryServiceSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }

    char copy[240] = {0};
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

    char fields[8][96];
    int field_count = kbo_csv_read_trimmed_line_fields(p, (char*)fields, sizeof(fields[0]), 8);
    return kbo_parse_military_service_seed_fields(fields, field_count, out);
}
