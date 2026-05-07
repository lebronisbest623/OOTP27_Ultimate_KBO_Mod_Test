static void kbo_hub_read_league_name(uintptr_t league_ptr, char* out, size_t out_size)
{
    if (league_ptr == 0 || out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char name[96] = {0};
    if (!copy_ootp_string_object_text((uint8_t*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, name, sizeof(name))) {
        append_logf("KBO: league name read failed ptr=%p offset=0x%x",
            (void*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET);
        return;
    }

    int score = kbo_hub_league_name_likeness_score(name);
    if (score < 30) {
        append_logf("KBO: league name rejected ptr=%p score=%d name=%s",
            (void*)league_ptr, score, name);
        return;
    }

    append_logf("KBO: league name read ptr=%p offset=0x%x score=%d name=%s",
        (void*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, score, name);
    snprintf(out, out_size, "%s", name);
}

static void kbo_hub_copy_league_display_name_fast(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x97\x86\xec\x9d\x8c", "No league"));
        return;
    }

    if (kbo_hub_try_copy_cached_league_name(league_id, out, out_size)) {
        return;
    }

    snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
}

static void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x97\x86\xec\x9d\x8c", "No league"));
        return;
    }

    if (kbo_hub_try_copy_cached_league_name(league_id, out, out_size)) {
        return;
    }

    uintptr_t league_ptr = kbo_find_league_ptr(league_id);
    if (league_ptr == 0) {
        snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
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
        snprintf(out, out_size, "%s %u / %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id, year);
    } else {
        snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
    }
}
