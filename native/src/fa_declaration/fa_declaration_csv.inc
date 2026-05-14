static int kbo_fa_declaration_append_csv(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA declaration csv open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    DWORD high = 0;
    DWORD size = GetFileSize(file, &high);
    int needs_header = high == 0u && size == 0u;
    if (needs_header) {
        kbo_fa_filing_write_raw(
            file,
            "date,season,player_id,name,declared,team_id,league_id,nation_id,age,contract_level,"
            "salary,fa_demand,score,threshold,grade,kbo_case,overall,talent,ratings,career,source,reason,decision\r\n");
    }

    for (int i = 0; i < candidate_count; i++) {
        const KboFaDeclarationCandidate* c = &candidates[i];
        if (c->player_id == 0u) {
            continue;
        }
        char prefix[192] = {0};
        snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,",
            c->declaration_date,
            c->season,
            c->player_id);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, c->player_name);
        snprintf(
            prefix,
            sizeof(prefix),
            ",%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,",
            (uint32_t)c->declared,
            c->team_id,
            c->league_id,
            c->nation_id,
            (uint32_t)c->age,
            (uint32_t)c->contract_level,
            c->salary,
            c->fa_demand,
            c->score,
            c->threshold);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, c->grade);
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, c->case_label);
        snprintf(
            prefix,
            sizeof(prefix),
            ",%d,%d,%d,%d,",
            (int)c->overall,
            (int)c->talent,
            (int)c->ratings,
            (int)c->career);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, source != NULL ? source : "fa_declaration_event");
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, c->reason);
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, c->decision_reason);
        kbo_fa_filing_write_raw(file, "\r\n");
    }

    CloseHandle(file);
    return 1;
}
