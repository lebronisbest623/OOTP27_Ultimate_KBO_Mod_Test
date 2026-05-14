static int kbo_json_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static int kbo_json_put_char(char** cursor, size_t* remaining, char ch)
{
    if (cursor == NULL || *cursor == NULL || remaining == NULL || *remaining == 0u) {
        return 0;
    }
    **cursor = ch;
    (*cursor)++;
    (*remaining)--;
    **cursor = '\0';
    return 1;
}

static int kbo_json_put_utf8(char** cursor, size_t* remaining, unsigned int codepoint)
{
    if (codepoint == 0u) {
        codepoint = '?';
    }
    if (codepoint <= 0x7Fu) {
        return kbo_json_put_char(cursor, remaining, (char)codepoint);
    }
    if (codepoint <= 0x7FFu) {
        return kbo_json_put_char(cursor, remaining, (char)(0xC0u | (codepoint >> 6)))
            && kbo_json_put_char(cursor, remaining, (char)(0x80u | (codepoint & 0x3Fu)));
    }
    return kbo_json_put_char(cursor, remaining, (char)(0xE0u | (codepoint >> 12)))
        && kbo_json_put_char(cursor, remaining, (char)(0x80u | ((codepoint >> 6) & 0x3Fu)))
        && kbo_json_put_char(cursor, remaining, (char)(0x80u | (codepoint & 0x3Fu)));
}

int kbo_json_string_value_at(const char* value, const char* end, char* out, size_t out_size)
{
    if (value == NULL || end == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    value = kbo_json_skip_ws(value, end);
    if (value >= end || *value != '"') {
        return 0;
    }

    const char* stop = kbo_json_find_string_end(value, end);
    if (stop == NULL || stop > end) {
        return 0;
    }

    char* cursor = out;
    size_t remaining = out_size - 1u;
    for (const char* p = value + 1; p < stop; p++) {
        if (*p != '\\') {
            if (!kbo_json_put_char(&cursor, &remaining, *p)) {
                return 0;
            }
            continue;
        }

        p++;
        if (p >= stop) {
            return 0;
        }
        switch (*p) {
        case '"':
        case '\\':
        case '/':
            if (!kbo_json_put_char(&cursor, &remaining, *p)) {
                return 0;
            }
            break;
        case 'b':
            if (!kbo_json_put_char(&cursor, &remaining, '\b')) {
                return 0;
            }
            break;
        case 'f':
            if (!kbo_json_put_char(&cursor, &remaining, '\f')) {
                return 0;
            }
            break;
        case 'n':
            if (!kbo_json_put_char(&cursor, &remaining, '\n')) {
                return 0;
            }
            break;
        case 'r':
            if (!kbo_json_put_char(&cursor, &remaining, '\r')) {
                return 0;
            }
            break;
        case 't':
            if (!kbo_json_put_char(&cursor, &remaining, '\t')) {
                return 0;
            }
            break;
        case 'u': {
            if (stop - p < 5) {
                return 0;
            }
            unsigned int codepoint = 0u;
            for (int i = 1; i <= 4; i++) {
                int hex = kbo_json_hex_value(p[i]);
                if (hex < 0) {
                    return 0;
                }
                codepoint = (codepoint << 4) | (unsigned int)hex;
            }
            if (!kbo_json_put_utf8(&cursor, &remaining, codepoint)) {
                return 0;
            }
            p += 4;
            break;
        }
        default:
            return 0;
        }
    }
    *cursor = '\0';
    return 1;
}
