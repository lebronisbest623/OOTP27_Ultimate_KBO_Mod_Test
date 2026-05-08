static int kbo_webview_asian_games_schedule(KboAsianGamesScheduleSeed* out)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        kbo_current_year_relaxed(&year);
    }
    if (year < 1982u || year > 2200u) {
        return 0;
    }
    return kbo_get_next_asian_games_schedule(year, out);
}

static void kbo_webview_format_asian_games_date(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_webview_format_asian_games_date_range(uint32_t start, uint32_t end, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (start == 0u && end == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    if (start == 0u || end == 0u || start == end) {
        kbo_webview_format_asian_games_date(start != 0u ? start : end, out, out_size);
        return;
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    kbo_webview_format_asian_games_date(start, start_text, sizeof(start_text));
    kbo_webview_format_asian_games_date(end, end_text, sizeof(end_text));
    snprintf(out, out_size, "%s - %s", start_text, end_text);
}
