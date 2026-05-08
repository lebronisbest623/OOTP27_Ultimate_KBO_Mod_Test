static uint32_t kbo_fa_filing_parse_u32(const char* text)
{
    if (text == NULL) {
        return 0u;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    if (*text == '\0') {
        return 0u;
    }
    char* tail = NULL;
    unsigned long value = strtoul(text, &tail, 0);
    if (tail == text) {
        return 0u;
    }
    return (uint32_t)value;
}

static void kbo_fa_filing_copy_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }
    size_t used = 0;
    while (text[used] != '\0' && used + 1u < out_size) {
        out[used] = text[used];
        used++;
    }
    out[used] = '\0';
}

static int kbo_fa_filing_parse_csv_field(char** cursor, char* out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }

    size_t used = 0;
    if (*p == '"') {
        p++;
        while (*p != '\0') {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (used + 1u < out_size) {
                        out[used++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                break;
            }
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (*p != '\0' && *p != ',' && *p != '\n') {
            p++;
        }
    } else {
        while (*p != '\0' && *p != ',' && *p != '\n') {
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (used > 0u && (out[used - 1u] == ' ' || out[used - 1u] == '\t' || out[used - 1u] == '\r')) {
            used--;
        }
    }

    out[used] = '\0';
    if (*p == ',') {
        p++;
    }
    *cursor = p;
    return 1;
}
