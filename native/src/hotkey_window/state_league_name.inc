static int kbo_league_name_char_ok(unsigned char c)
{
    /* Allow printable ASCII (0x20-0x7E) and all UTF-8 continuation bytes/multi-byte chars (0x80+)
     * This more permissive check allows Korean and other multi-byte UTF-8 sequences. */
    return (c >= 0x20u && c <= 0x7Eu) || c >= 0x80u;
}

/* Scans common string-object offsets within a league object and copies the
 * first string that looks like a league name into out. */
static void kbo_hub_read_league_name(uintptr_t league_ptr, char* out, size_t out_size)
{
    if (league_ptr == 0 || out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    static const uint32_t try_offsets[] = {
        0x00u, 0x08u, 0x10u, 0x18u, 0x20u, 0x28u, 0x30u, 0x38u,
        0x40u, 0x48u, 0x50u, 0x58u, 0x60u, 0x68u, 0x70u, 0x78u,
        0x80u, 0x88u, 0x90u, 0x98u, 0xa0u, 0xa8u, 0xb0u, 0xb8u,
        0xc0u, 0xc8u, 0xd0u, 0xd8u, 0xe0u, 0xe8u, 0xf0u, 0xf8u,
    };

    uint8_t* league = (uint8_t*)league_ptr;
    char text[128]  = {0};
    append_logf("KBO: scanning league name at %p", league_ptr);

    for (size_t i = 0; i < sizeof(try_offsets) / sizeof(try_offsets[0]); i++) {
        text[0] = '\0';
        if (!copy_ootp_string_object_text(league, try_offsets[i], text, sizeof(text))) {
            append_logf("KBO:   offset 0x%02x - copy_ootp_string_object_text failed", try_offsets[i]);
            continue;
        }
        size_t len = strlen(text);
        append_logf("KBO:   offset 0x%02x - got text len=%zu: %s", try_offsets[i], len, text);
        
        if (len < 2 || len > 60) {
            append_logf("KBO:   offset 0x%02x - REJECT: length check failed (len=%zu)", try_offsets[i], len);
            continue;
        }
        /* Must start with a letter (ASCII or UTF-8 multi-byte). */
        unsigned char first = (unsigned char)text[0];
        if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first >= 0x80u)) {
            append_logf("KBO:   offset 0x%02x - REJECT: first char not a letter (0x%02x)", try_offsets[i], first);
            continue;
        }
        /* All bytes must be printable ASCII or UTF-8 continuation. */
        int clean = 1;
        for (size_t j = 0; j < len; j++) {
            if (!kbo_league_name_char_ok((unsigned char)text[j])) {
                append_logf("KBO:   offset 0x%02x - REJECT: invalid char at pos %zu (0x%02x)", 
                    try_offsets[i], j, (unsigned char)text[j]);
                clean = 0; break;
            }
        }
        if (!clean) {
            continue;
        }
        append_logf("KBO:   offset 0x%02x - ACCEPT: %s", try_offsets[i], text);
        snprintf(out, out_size, "%s", text);
        return;
    }
    append_logf("KBO: league name scan FAILED - no valid string found at %p", league_ptr);
}

static void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("由ш렇 ?놁쓬", "No league"));
        return;
    }

    uintptr_t league_ptr = kbo_find_league_ptr(league_id);
    if (league_ptr == 0) {
        snprintf(out, out_size, "%s %u", kbo_hub_text("由ш렇", "League"), league_id);
        return;
    }

    char name[96] = {0};
    kbo_hub_read_league_name(league_ptr, name, sizeof(name));

    uint32_t year = 0;
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }

    if (name[0] != '\0') {
        snprintf(out, out_size, "%s", name);
    } else if (year >= 1982u && year <= 2100u) {
        snprintf(out, out_size, "%s %u / %u", kbo_hub_text("由ш렇", "League"), league_id, year);
    } else {
        snprintf(out, out_size, "%s %u", kbo_hub_text("由ш렇", "League"), league_id);
    }
}
