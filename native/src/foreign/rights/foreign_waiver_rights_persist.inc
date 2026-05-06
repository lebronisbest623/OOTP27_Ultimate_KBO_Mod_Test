/* Foreign reserve-right CSV persistence. */

static int kbo_persist_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        append_log_line("foreign reserve rights: persist skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
        append_logf("foreign reserve rights: persist failed reason=create_tmp gle=%lu path=%s", GetLastError(), path);
        return 0;
    }
    char header[64] = "player_id,team_id,league_id,retained_on,expires_on\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == 0u || rec->team_id == 0u) {
            continue;
        }
        char line[128] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u\r\n",
            rec->player_id,
            rec->team_id,
            rec->league_id,
            rec->retained_on_yyyymmdd,
            rec->expires_on_yyyymmdd);
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    int ok = kbo_atomic_commit(file, tmp_path, path);
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    if (!ok) {
        append_logf("foreign reserve rights: atomic commit failed path=%s", path);
        return 0;
    }
    append_logf("foreign reserve rights: persisted=%d path=%s", g_kbo_foreign_waiver_rights_count, path);
    return 1;
}
