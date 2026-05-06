/* Foreign injury replacement CSV persistence. Included from foreign_injury_replacement.inc. */

static int kbo_persist_foreign_injury_replacements_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign injury replacement: persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header = "team_id,league_id,injured_player_id,replacement_player_id,opened_on,expected_end,slot_type,status,converted\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        char line[256] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            rec->team_id,
            rec->league_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            rec->opened_on_yyyymmdd,
            rec->expected_end_yyyymmdd,
            (uint32_t)rec->slot_type,
            (uint32_t)rec->status,
            (uint32_t)rec->converted);
        if (len > 0 && len < (int)sizeof(line)) {
            written = 0;
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        append_logf("foreign injury replacement: atomic commit failed path=%s", path);
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);
    return 1;
}

