static void kbo_build_fa_cases_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    if (rows == NULL) {
        kbo_window_text_appendf(&buffer, "FA CASES\r\n\r\nCould not allocate classification buffer.\r\n");
        return;
    }

    KboFaMarketScanSummary summary = {0};
    int count = kbo_collect_fa_market_classifications(
        g_kbo_hub_selected_league_id,
        rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &summary,
        1,
        "f2_text");

    char league_name[128] = {0};
    kbo_hub_copy_league_display_name(summary.league_id, league_name, sizeof(league_name));

    kbo_window_text_appendf(&buffer, "FA CASES\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n", league_name[0] != '\0' ? league_name : "Selected league");
    kbo_window_text_appendf(
        &buffer,
        "Scanned %d players / %d active teamless candidates / %d rows. Seed rows: %d. CSV: %s\r\n\r\n",
        summary.scanned,
        summary.candidates,
        count,
        summary.seed_count,
        summary.csv_path[0] != '\0' ? summary.csv_path : "-");
    kbo_window_text_appendf(&buffer, "PLAYER                   CASE            GRADE    PREV SALARY TEAM AGE RIGHTS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < count && i < 600; i++) {
        KboFaMarketClassification* row = &rows[i];
        char team_abbrev[16] = "-";
        char rights_abbrev[16] = "-";
        char salary_text[32] = "-";
        kbo_fa_market_format_salary(row->fa_grade_salary, salary_text, sizeof(salary_text));
        kbo_hub_copy_team_abbrev_by_id(
            kbo_fa_market_display_team_id(row),
            team_abbrev,
            sizeof(team_abbrev),
            "-");
        kbo_hub_copy_team_abbrev_by_id(row->rights_team_id, rights_abbrev, sizeof(rights_abbrev), "-");
        kbo_window_text_appendf(
            &buffer,
            "%-24.24s %-15.15s %-5.5s %14s %-4.4s %3u %-6.6s\r\n",
            row->player_name,
            kbo_fa_market_display_case_label(row->case_label),
            kbo_fa_market_display_grade(row->grade),
            salary_text,
            team_abbrev,
            (uint32_t)row->age,
            rights_abbrev);
    }
    if (count == 0) {
        kbo_window_text_appendf(&buffer, "No active players without a current team found.\r\n");
    } else if (summary.truncated || count > 600) {
        kbo_window_text_appendf(&buffer, "\r\nOutput truncated. Open the CSV for the full snapshot.\r\n");
    }

    HeapFree(GetProcessHeap(), 0, rows);
}
