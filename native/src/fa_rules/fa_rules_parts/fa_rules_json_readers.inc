static const char* kbo_fa_rules_json_value_end(const char* value, const char* end)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end) {
        return value;
    }
    if (*value == '"') {
        const char* stop = kbo_json_find_string_end(value, end);
        return stop != NULL ? stop + 1 : value;
    }
    if (*value == '{' || *value == '[') {
        char open = *value;
        char close = open == '{' ? '}' : ']';
        int depth = 0;
        int in_string = 0;
        int escaped = 0;
        const char* p = value;
        while (p < end) {
            char ch = *p;
            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (ch == '\\') {
                    escaped = 1;
                } else if (ch == '"') {
                    in_string = 0;
                }
                p++;
                continue;
            }
            if (ch == '"') {
                in_string = 1;
                p++;
                continue;
            }
            if (ch == open) {
                depth++;
            } else if (ch == close) {
                depth--;
                if (depth == 0) {
                    return p + 1;
                }
            }
            p++;
        }
        return value;
    }

    const char* p = value;
    while (p < end && *p != ',' && *p != '}' && *p != ']') {
        p++;
    }
    while (p > value && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r' || p[-1] == '\n')) {
        p--;
    }
    return p;
}

static int kbo_fa_rules_find_json_value_span(
    const char* start,
    const char* end,
    const char* key,
    const char** out_start,
    const char** out_end)
{
    if (start == NULL || end == NULL || key == NULL || out_start == NULL || out_end == NULL) {
        return 0;
    }

    const char* p = start;
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
        const char* value_start = kbo_json_skip_ws(colon + 1, end);
        const char* value_end = kbo_fa_rules_json_value_end(value_start, end);
        if (value_start >= end || value_end < value_start) {
            return 0;
        }
        *out_start = value_start;
        *out_end = value_end;
        return 1;
    }
    return 0;
}

static int kbo_fa_rules_read_bool(const char* start, const char* end, const char* key, uint8_t* out_value)
{
    const char* value_start = NULL;
    const char* value_end = NULL;
    int value = 0;
    if (out_value == NULL
            || !kbo_fa_rules_find_json_value_span(start, end, key, &value_start, &value_end)
            || !kbo_json_bool_value_at(value_start, value_end, &value)) {
        return 0;
    }
    *out_value = value ? 1u : 0u;
    return 1;
}

static int kbo_fa_rules_read_u32(const char* start, const char* end, const char* key, uint32_t* out_value)
{
    const char* value_start = NULL;
    const char* value_end = NULL;
    int value = 0;
    if (out_value == NULL
            || !kbo_fa_rules_find_json_value_span(start, end, key, &value_start, &value_end)
            || !kbo_json_int_value_at(value_start, value_end, &value)
            || value < 0) {
        return 0;
    }
    *out_value = (uint32_t)value;
    return 1;
}

static int kbo_fa_rules_copy_json_string_value(
    const char* value_start,
    const char* value_end,
    char* out,
    size_t out_size)
{
    if (value_start == NULL || value_end == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    value_start = kbo_json_skip_ws(value_start, value_end);
    if (value_start >= value_end || *value_start != '"') {
        return 0;
    }
    const char* stop = kbo_json_find_string_end(value_start, value_end);
    if (stop == NULL) {
        return 0;
    }

    size_t written = 0;
    const char* p = value_start + 1;
    while (p < stop && written + 1u < out_size) {
        if (*p == '\\' && p + 1 < stop) {
            p++;
        }
        out[written++] = *p++;
    }
    out[written] = '\0';
    return out[0] != '\0';
}

static int kbo_fa_rules_read_string(
    const char* start,
    const char* end,
    const char* key,
    char* out,
    size_t out_size)
{
    const char* value_start = NULL;
    const char* value_end = NULL;
    return kbo_fa_rules_find_json_value_span(start, end, key, &value_start, &value_end)
        && kbo_fa_rules_copy_json_string_value(value_start, value_end, out, out_size);
}
