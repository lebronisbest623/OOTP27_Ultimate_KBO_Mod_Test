/* Military service CSV token and date parsing helpers. Included from native/src/military_service_loan.inc. */

static void kbo_military_trim_csv_token_in_place(char* text)
{
    if (text == NULL) {
        return;
    }
    char* start = text;
    while (*start == ' ' || *start == '\t' || *start == '"') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start
            && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'
                || end[-1] == '\n' || end[-1] == '"')) {
        end--;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }
}

static int kbo_military_ascii_is_seed_id_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static int kbo_military_parse_u32_full_token(const char* text, uint32_t* out)
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

    unsigned long value = 0ul;
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10ul + (unsigned long)(*p - '0');
        if (value > 0xfffffffful) {
            return 0;
        }
    }

    *out = (uint32_t)value;
    return 1;
}

static uint32_t kbo_military_parse_yyyymmdd(const char* text)
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
    uint32_t month = (value / 100u) % 100u;
    uint32_t day = value % 100u;
    return (month >= 1u && month <= 12u && day >= 1u && day <= 31u) ? value : 0u;
}

static uint32_t kbo_military_yyyymmdd_to_serial(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return 0u;
    }
    return kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
}

static uint32_t kbo_military_days_in_month(uint32_t year, uint32_t month)
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

static uint32_t kbo_military_serial_to_yyyymmdd(uint32_t serial)
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

static uint32_t kbo_military_yyyymmdd_add_days(uint32_t yyyymmdd, int32_t days)
{
    uint32_t serial = kbo_military_yyyymmdd_to_serial(yyyymmdd);
    if (serial == 0u || days < 0) {
        return 0u;
    }
    return kbo_military_serial_to_yyyymmdd(serial + (uint32_t)days);
}

static int32_t kbo_military_days_left_from_return_serial(uint32_t return_serial, uint32_t today_serial)
{
    if (return_serial == 0u || today_serial == 0u || return_serial <= today_serial) {
        return 0;
    }
    uint32_t diff = return_serial - today_serial;
    return diff > 32767u ? 32767 : (int32_t)diff;
}


