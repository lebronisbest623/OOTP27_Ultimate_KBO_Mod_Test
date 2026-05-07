#define KBO_FLAGS_JSON_MAX_BYTES (64u * 1024u)

static int kbo_ascii_lower_char(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int kbo_text_equals_ignore_case_n(const char* text, size_t len, const char* expected)
{
    if (text == NULL || expected == NULL) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (expected[i] == '\0'
                || kbo_ascii_lower_char((unsigned char)text[i]) != kbo_ascii_lower_char((unsigned char)expected[i])) {
            return 0;
        }
    }
    return expected[len] == '\0';
}
static const char* kbo_json_skip_ws(const char* p, const char* end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    return p;
}

static const char* kbo_json_find_string_end(const char* quote, const char* end)
{
    if (quote == NULL || quote >= end || *quote != '"') {
        return NULL;
    }

    const char* p = quote + 1;
    while (p < end) {
        if (*p == '\\') {
            p += (p + 1 < end) ? 2 : 1;
            continue;
        }
        if (*p == '"') {
            return p;
        }
        p++;
    }
    return NULL;
}

static int kbo_json_string_equals_key(const char* start, const char* stop, const char* key)
{
    if (start == NULL || stop == NULL || key == NULL) {
        return 0;
    }

    const char* p = start;
    const char* k = key;
    while (p < stop && *k != '\0') {
        if (*p == '\\' || *p != *k) {
            return 0;
        }
        p++;
        k++;
    }
    return p == stop && *k == '\0';
}

static int kbo_json_bool_text_value(const char* text, size_t len, int* out_value)
{
    if (out_value == NULL) {
        return 0;
    }
    if (kbo_text_equals_ignore_case_n(text, len, "1")
            || kbo_text_equals_ignore_case_n(text, len, "true")
            || kbo_text_equals_ignore_case_n(text, len, "yes")
            || kbo_text_equals_ignore_case_n(text, len, "on")
            || kbo_text_equals_ignore_case_n(text, len, "enabled")) {
        *out_value = 1;
        return 1;
    }
    if (kbo_text_equals_ignore_case_n(text, len, "0")
            || kbo_text_equals_ignore_case_n(text, len, "false")
            || kbo_text_equals_ignore_case_n(text, len, "no")
            || kbo_text_equals_ignore_case_n(text, len, "off")
            || kbo_text_equals_ignore_case_n(text, len, "disabled")) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int kbo_json_bool_value_at(const char* value, const char* end, int* out_value)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end || out_value == NULL) {
        return 0;
    }

    if (*value == '"') {
        const char* stop = kbo_json_find_string_end(value, end);
        return stop != NULL && kbo_json_bool_text_value(value + 1, (size_t)(stop - value - 1), out_value);
    }
    if (end - value >= 4 && kbo_text_equals_ignore_case_n(value, 4, "true")) {
        *out_value = 1;
        return 1;
    }
    if (end - value >= 5 && kbo_text_equals_ignore_case_n(value, 5, "false")) {
        *out_value = 0;
        return 1;
    }
    if ((*value >= '0' && *value <= '9') || *value == '-' || *value == '+') {
        int digit_seen = 0;
        int nonzero = 0;
        const char* p = value;
        if (p < end && (*p == '-' || *p == '+')) {
            p++;
        }
        while (p < end && *p >= '0' && *p <= '9') {
            digit_seen = 1;
            if (*p != '0') {
                nonzero = 1;
            }
            p++;
        }
        *out_value = nonzero ? 1 : 0;
        return digit_seen;
    }

    return 0;
}

static int kbo_json_int_text_value(const char* text, size_t len, int* out_value)
{
    if (text == NULL || out_value == NULL || len == 0) {
        return 0;
    }

    int sign = 1;
    size_t i = 0;
    while (i < len && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
        i++;
    }
    if (i >= len) {
        return 0;
    }
    if (text[i] == '-' || text[i] == '+') {
        sign = text[i] == '-' ? -1 : 1;
        i++;
    }
    if (i >= len || text[i] < '0' || text[i] > '9') {
        return 0;
    }

    long long value = 0;
    while (i < len && text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (long long)(text[i] - '0');
        if (value > 2147483647LL) {
            return 0;
        }
        i++;
    }
    while (i < len && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
        i++;
    }
    if (i != len) {
        return 0;
    }

    value *= sign;
    if (value < -2147483647LL - 1LL || value > 2147483647LL) {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

static int kbo_json_int_value_at(const char* value, const char* end, int* out_value)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end || out_value == NULL) {
        return 0;
    }

    if (*value == '"') {
        const char* stop = kbo_json_find_string_end(value, end);
        return stop != NULL && kbo_json_int_text_value(value + 1, (size_t)(stop - value - 1), out_value);
    }

    const char* stop = value;
    if (stop < end && (*stop == '-' || *stop == '+')) {
        stop++;
    }
    while (stop < end && *stop >= '0' && *stop <= '9') {
        stop++;
    }
    return kbo_json_int_text_value(value, (size_t)(stop - value), out_value);
}

static int kbo_find_flag_value_in_json(const char* json, DWORD json_size, const char* key, const char* legacy_key, int* out_value)
{
    if (json == NULL || key == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = json;
    const char* end = json + json_size;
    while (p < end) {
        if (*p != '"') {
            p++;
            continue;
        }

        const char* key_start = p + 1;
        const char* key_stop = kbo_json_find_string_end(p, end);
        if (key_stop == NULL) {
            return 0;
        }

        int matched = kbo_json_string_equals_key(key_start, key_stop, key)
            || (legacy_key != NULL && kbo_json_string_equals_key(key_start, key_stop, legacy_key));
        p = key_stop + 1;
        if (!matched) {
            continue;
        }

        const char* colon = kbo_json_skip_ws(p, end);
        if (colon >= end || *colon != ':') {
            continue;
        }
        if (kbo_json_bool_value_at(colon + 1, end, out_value)) {
            return 1;
        }
    }

    return 0;
}

static int kbo_find_int_value_in_json(const char* json, DWORD json_size, const char* key, int* out_value)
{
    if (json == NULL || key == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = json;
    const char* end = json + json_size;
    while (p < end) {
        if (*p != '"') {
            p++;
            continue;
        }

        const char* key_start = p + 1;
        const char* key_stop = kbo_json_find_string_end(p, end);
        if (key_stop == NULL) {
            return 0;
        }

        int matched = kbo_json_string_equals_key(key_start, key_stop, key);
        p = key_stop + 1;
        if (!matched) {
            continue;
        }

        const char* colon = kbo_json_skip_ws(p, end);
        if (colon >= end || *colon != ':') {
            continue;
        }
        if (kbo_json_int_value_at(colon + 1, end, out_value)) {
            return 1;
        }
    }

    return 0;
}
