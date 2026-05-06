/* String helpers (from league_team_rules) */

static int copy_limited_ascii_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (source == NULL || !memory_range_readable(source, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 512; len++) {
        if (!memory_range_readable(source + len, 1)) {
            break;
        }
        unsigned char c = (unsigned char)source[len];
        if (c == '\0') {
            break;
        }
        if (c < 0x20u || c > 0x7eu) {
            break;
        }
        out[len] = (char)c;
    }

    out[len] = '\0';
    return len > 0;
}

static int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size)
{
    uint8_t* string_object = object_base + string_offset;
    uint8_t* text_slot     = string_object + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET;
    if (!memory_range_readable(text_slot, sizeof(char*))) {
        return 0;
    }

    const char* text = *(const char**)text_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static int team_has_ootp_string_text(uint8_t* team, const char* expected)
{
    if (team == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }

    static const uint32_t string_offsets[] = {
        0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u
    };

    char text[96] = {0};
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        text[0] = '\0';
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && ascii_equals_ignore_case(text, expected)) {
            return 1;
        }
    }

    return 0;
}
