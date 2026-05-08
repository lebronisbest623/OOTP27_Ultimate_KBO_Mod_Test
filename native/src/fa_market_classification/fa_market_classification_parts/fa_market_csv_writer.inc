static void kbo_fa_market_write_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

static void kbo_fa_market_write_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    kbo_fa_market_write_raw(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            char ch = *p;
            if (ch == '"') {
                kbo_fa_market_write_raw(file, "\"\"");
            } else {
                char one[2] = { ch, '\0' };
                kbo_fa_market_write_raw(file, one);
            }
        }
    }
    kbo_fa_market_write_raw(file, "\"");
}

static void kbo_write_fa_market_classification_csv(
    const KboFaMarketClassification* rows,
    int row_count,
    const KboFaMarketScanSummary* summary,
    const char* source)
{
    if (rows == NULL || row_count < 0 || summary == NULL) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_market_classification_csv_path(path, sizeof(path))) {
        append_log_line("FA market classification: unable to resolve CSV path");
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("FA market classification: failed to open CSV path=%s gle=%lu", path, GetLastError());
        return;
    }

    kbo_fa_market_write_raw(
        file,
        "date,source,selected_league_id,player_id,name,nation_id,current_team_id,active_team_id,original_team_id,current_league_id,draft_league_id,age,retired_flag,contract_level,fa_demand,dfa_flag,foreign_flag,draft_class,draft_subtype,draft_eligible,draft_extra,generation_flags,generation_context,generation_grade,generation_special,rights_team_id,kbo_case,kbo_grade,fa_grade_salary,fa_grade_overall_rank,fa_grade_team_rank,fa_grade_snapshot_team_id,fa_grade_snapshot_date,fa_grade_opening_day,fa_grade_auto,fa_grade_team_changed_review,fa_grade_flag,reason\r\n");

    char date[16] = {0};
    if (summary->today_yyyymmdd != 0u) {
        snprintf(date, sizeof(date), "%08u", summary->today_yyyymmdd);
    } else if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        snprintf(date, sizeof(date), "00000000");
    }

    for (int i = 0; i < row_count; i++) {
        const KboFaMarketClassification* row = &rows[i];
        char prefix[256] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%s,%s,%u,%u,",
            date,
            source != NULL ? source : "",
            summary->league_id,
            row->player_id);
        if (len <= 0 || len >= (int)sizeof(prefix)) {
            continue;
        }
        kbo_fa_market_write_raw(file, prefix);
        kbo_fa_market_write_csv_text(file, row->player_name);
        char middle[320] = {0};
        len = snprintf(
            middle,
            sizeof(middle),
            ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,",
            row->nation_id,
            row->current_team_id,
            row->active_team_id,
            row->original_team_id,
            row->current_league_id,
            row->draft_league_id,
            (uint32_t)row->age,
            (uint32_t)row->retired_flag,
            (uint32_t)row->contract_level,
            row->fa_demand,
            (uint32_t)row->dfa,
            (uint32_t)row->foreign_player,
            (uint32_t)row->draft_class,
            (uint32_t)row->draft_subtype,
            (uint32_t)row->draft_eligible,
            (uint32_t)row->draft_extra,
            (uint32_t)row->generation_flags,
            (uint32_t)row->generation_context,
            (uint32_t)row->generation_grade,
            (uint32_t)row->generation_special,
            row->rights_team_id);
        if (len <= 0 || len >= (int)sizeof(middle)) {
            continue;
        }
        kbo_fa_market_write_raw(file, middle);
        kbo_fa_market_write_csv_text(file, row->case_label);
        kbo_fa_market_write_raw(file, ",");
        kbo_fa_market_write_csv_text(file, row->grade);
        char grade_middle[192] = {0};
        len = snprintf(
            grade_middle,
            sizeof(grade_middle),
            ",%d,%u,%u,%u,%u,%u,%u,%u,",
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank,
            row->fa_grade_snapshot_team_id,
            row->fa_grade_snapshot_date,
            row->fa_grade_opening_day,
            (uint32_t)row->fa_grade_auto,
            (uint32_t)row->fa_grade_team_changed_review);
        if (len <= 0 || len >= (int)sizeof(grade_middle)) {
            continue;
        }
        kbo_fa_market_write_raw(file, grade_middle);
        kbo_fa_market_write_csv_text(file, row->fa_grade_flag);
        kbo_fa_market_write_raw(file, ",");
        kbo_fa_market_write_csv_text(file, row->reason);
        kbo_fa_market_write_raw(file, "\r\n");
    }

    CloseHandle(file);
    append_logf(
        "FA market classification: rows=%d candidates=%d scanned=%d league=%u salary_snapshot=%d csv=%s",
        row_count,
        summary->candidates,
        summary->scanned,
        summary->league_id,
        summary->salary_snapshot_count,
        path);
}
