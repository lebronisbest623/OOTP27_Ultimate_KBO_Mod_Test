#include "core_text_date.h"

#include <stdio.h>

int ascii_equals_ignore_case(const char* a, const char* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

int kbo_is_leap_year(uint32_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || (year % 400u == 0u);
}

uint32_t kbo_date_serial(uint32_t year, uint32_t month, uint32_t day)
{
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    static const uint8_t days_by_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }
    uint32_t max_day = days_by_month[month - 1u];
    if (month == 2u && kbo_is_leap_year(year)) {
        max_day = 29u;
    }
    if (day > max_day) {
        return 0;
    }

    uint32_t y = year - 1u;
    uint32_t serial = y * 365u + y / 4u - y / 100u + y / 400u;
    serial += days_before_month[month - 1u];
    if (month > 2 && kbo_is_leap_year(year)) {
        serial++;
    }
    serial += day;
    return serial;
}

int kbo_format_history_date(char* out, size_t out_size, uint32_t year, uint32_t month, uint32_t day)
{
    static const uint8_t month_days_common[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (out == NULL || out_size < 9 || year < 1800 || year > 2200
            || month < 1 || month > 12 || day < 1) {
        return 0;
    }

    uint32_t max_day = month_days_common[month - 1u];
    if (month == 2 && kbo_is_leap_year(year)) {
        max_day = 29;
    }
    if (day > max_day) {
        return 0;
    }

    snprintf(out, out_size, "%04u%02u%02u", year, month, day);
    return 1;
}
