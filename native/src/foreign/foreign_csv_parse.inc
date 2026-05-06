/* Shared foreign-policy CSV field parsing. Included from native/src/foreign_waiver_ai.inc. */

static int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value)
{
    if (cursor == NULL || *cursor == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == ',') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    unsigned long long value = 0;
    while (*p >= '0' && *p <= '9') {
        value = (value * 10ULL) + (unsigned long long)(*p - '0');
        if (value > UINT32_MAX) {
            return 0;
        }
        p++;
    }

    *out_value = (uint32_t)value;
    *cursor = p;
    return 1;
}

