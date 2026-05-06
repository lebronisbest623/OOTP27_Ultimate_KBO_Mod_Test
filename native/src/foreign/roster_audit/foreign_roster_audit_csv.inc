/* Foreign roster audit CSV writers. Included from foreign_roster_audit.inc. */

static int append_foreign_roster_audit_csv_header(HANDLE file)
{
    const char* header =
        "date,source,change_type,player_id,nation_id,"
        "old_current_team_id,new_current_team_id,"
        "old_active_team_id,new_active_team_id,"
        "old_original_team_id,new_original_team_id,"
        "old_current_league_id,new_current_league_id,"
        "old_loan_team_id,new_loan_team_id,"
        "old_restricted,new_restricted,"
        "old_secondary_restricted,new_secondary_restricted,"
        "old_dfa,new_dfa,"
        "old_loan_active,new_loan_active,"
        "old_injury_active,new_injury_active,"
        "old_foreign_value_score,new_foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static int append_foreign_roster_snapshot_csv_header(HANDLE file)
{
    const char* header =
        "date,source,player_id,nation_id,current_team_id,active_team_id,original_team_id,"
        "current_league_id,loan_team_id,restricted,secondary_restricted,dfa,loan_active,"
        "injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static HANDLE kbo_open_foreign_roster_audit_append_file(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_roster_audit_csv_path(path, sizeof(path))) {
        append_log_line("foreign roster audit: unable to resolve audit output path");
        return INVALID_HANDLE_VALUE;
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign roster audit: failed to open audit file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    if (needs_header || is_csv_empty(file)) {
        append_foreign_roster_audit_csv_header(file);
    }
    return file;
}

static void kbo_write_foreign_roster_audit_change(
    HANDLE file,
    const char* date,
    const char* source,
    const char* change_type,
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (file == INVALID_HANDLE_VALUE || new_state == NULL) {
        return;
    }

    KboForeignRosterAuditState zero = {0};
    if (old_state == NULL) {
        old_state = &zero;
    }

    char line[512] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        change_type != NULL ? change_type : "",
        new_state->player_id,
        new_state->nation_id,
        old_state->current_team_id,
        new_state->current_team_id,
        old_state->active_team_id,
        new_state->active_team_id,
        old_state->original_team_id,
        new_state->original_team_id,
        old_state->current_league_id,
        new_state->current_league_id,
        old_state->loan_team_id,
        new_state->loan_team_id,
        (uint32_t)old_state->restricted,
        (uint32_t)new_state->restricted,
        (uint32_t)old_state->secondary_restricted,
        (uint32_t)new_state->secondary_restricted,
        (uint32_t)old_state->dfa,
        (uint32_t)new_state->dfa,
        (uint32_t)old_state->loan_active,
        (uint32_t)new_state->loan_active,
        (uint32_t)old_state->injury_active,
        (uint32_t)new_state->injury_active,
        old_state->score,
        new_state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static void kbo_write_foreign_roster_snapshot_row(
    HANDLE file,
    const char* date,
    const char* source,
    const KboForeignRosterAuditState* state)
{
    if (file == INVALID_HANDLE_VALUE || state == NULL) {
        return;
    }

    char line[320] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        state->player_id,
        state->nation_id,
        state->current_team_id,
        state->active_team_id,
        state->original_team_id,
        state->current_league_id,
        state->loan_team_id,
        (uint32_t)state->restricted,
        (uint32_t)state->secondary_restricted,
        (uint32_t)state->dfa,
        (uint32_t)state->loan_active,
        (uint32_t)state->injury_active,
        state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static HANDLE kbo_open_foreign_roster_snapshot_file(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_roster_snapshot_csv_path(path, sizeof(path))) {
        append_log_line("foreign roster audit: unable to resolve snapshot output path");
        return INVALID_HANDLE_VALUE;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign roster audit: failed to open snapshot file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    append_foreign_roster_snapshot_csv_header(file);
    return file;
}

