/* Foreign waiver status text for hotkey UI. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_format_ymd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size < 11u) {
        return 0;
    }
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year == 0u || month == 0u || day == 0u
            || month > 12u || day > 31u || year < 1980u) {
        return 0;
    }
    return (snprintf(out, out_size, "%04u-%02u-%02u", year, month, day) > 0) ? 1 : 0;
}

static int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    if (!kbo_foreign_waiver_ai_enabled()) {
        snprintf(out, out_size, "Foreign waiver negotiation: disabled (set enable_foreign_waiver_ai=true in kbo_flags.json)");
        return 1;
    }

    if (!kbo_fix_enabled()) {
        snprintf(out, out_size, "Foreign waiver negotiation: disabled (KBO fix off)");
        return 1;
    }

    uint32_t today = 0;
    if (!kbo_get_current_yyyymmdd(&today)) {
        snprintf(out, out_size, "Foreign waiver negotiation: unavailable (failed to read game date)");
        return 1;
    }
    int window_open = kbo_is_foreign_waiver_negotiation_window_open();
    uint32_t file_start = 0;
    uint32_t file_end = 0;
    const char* source = "auto detector";
    if (kbo_read_foreign_waiver_window(&file_start, &file_end)) {
        source = "override file";
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (!kbo_format_ymd(file_start, start_text, sizeof(start_text))
            || !kbo_format_ymd(file_end, end_text, sizeof(end_text))) {
        snprintf(start_text, sizeof(start_text), "-");
        snprintf(end_text, sizeof(end_text), "-");
    }

    const char* status = window_open ? "OPEN" : "CLOSED";
    snprintf(
        out,
        out_size,
        "Foreign waiver priority window [league=%u] (%s): %s [Window=%s ~ %s]",
        configured_league_id,
        source,
        status,
        start_text,
        end_text);
    return 1;
}

