/* Core ASCII text and date formatting helpers. */

static int ascii_equals_ignore_case(const char* a, const char* b)
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

static int kbo_is_leap_year(uint32_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || (year % 400u == 0u);
}

static int kbo_format_history_date(char* out, size_t out_size, uint32_t year, uint32_t month, uint32_t day)
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
