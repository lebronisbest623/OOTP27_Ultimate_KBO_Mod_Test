/* Asian Games roster CSV persistence. Included from native/src/custom_events.inc. */

static int kbo_get_asian_games_roster_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_save_scoped_data_file("asian_games_roster.csv", out, out_size);
}

static int kbo_get_legacy_asian_games_roster_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s\\OOTP-KBO", local_app_data);
    CreateDirectoryA(dir, NULL);
    snprintf(out, out_size, "%s\\asian_games_roster.csv", dir);
    return out[0] != '\0';
}

static void kbo_clear_asian_games_roster_memory(const char* source)
{
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = 0;
    g_kbo_asian_games_roster_save_path[0] = '\0';
    append_logf("KBO Asian Games roster memory cleared source=%s", source != NULL ? source : "");
}

static void kbo_clear_asian_games_roster_if_save_changed(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        if (g_kbo_asian_games_roster_count > 0 || g_kbo_asian_games_roster_save_path[0] != '\0') {
            kbo_clear_asian_games_roster_memory(source);
        }
        return;
    }

    if (g_kbo_asian_games_roster_save_path[0] != '\0'
            && _stricmp(g_kbo_asian_games_roster_save_path, path) != 0) {
        kbo_clear_asian_games_roster_memory(source);
    }
}

static int kbo_save_asian_games_roster_csv(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        append_logf("KBO Asian Games roster csv save skipped source=%s reason=path_unavailable", source != NULL ? source : "");
        return 0;
    }

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO Asian Games roster csv save skipped source=%s reason=create_failed gle=%lu path=%s", source != NULL ? source : "", GetLastError(), path);
        return 0;
    }

    const char* header = "year,index,player_id,original_team_id,original_league_id,departure_date,return_date,age,role,wildcard,old_restricted,old_secondary_restricted,old_injury_active,old_injury_days_left,departed,returned,exempted,score\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    int ok = 1;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        char line[512] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d\r\n",
            g_kbo_asian_games_roster_year,
            i + 1,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->departure_date,
            entry->return_date,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            (uint32_t)entry->wildcard,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left,
            (uint32_t)entry->departed,
            (uint32_t)entry->returned,
            (uint32_t)entry->exempted,
            entry->score);
        if (len <= 0 || !WriteFile(file, line, (DWORD)len, &written, NULL) || written != (DWORD)len) {
            ok = 0;
            break;
        }
    }

    if (!ok || !kbo_atomic_commit(file, tmp_path, path)) {
        if (ok) {
            append_logf("KBO Asian Games roster csv save: atomic commit failed path=%s", path);
        }
        if (!ok) {
            kbo_atomic_commit(file, tmp_path, path);
            DeleteFileA(path);
        }
        return 0;
    }
    if (ok) {
        snprintf(g_kbo_asian_games_roster_save_path, sizeof(g_kbo_asian_games_roster_save_path), "%s", path);
    }
    append_logf("KBO Asian Games roster csv save source=%s ok=%d year=%u count=%ld path=%s", source != NULL ? source : "", ok, g_kbo_asian_games_roster_year, roster_count, path);
    return ok;
}

static int kbo_load_asian_games_roster_csv(const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        kbo_clear_asian_games_roster_memory(source);
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD scoped_error = GetLastError();
        kbo_clear_asian_games_roster_memory(source);
        append_logf("KBO Asian Games roster csv load skipped source=%s reason=open_failed gle=%lu path=%s", source != NULL ? source : "", scoped_error, path);
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 65536u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int ok = ReadFile(file, raw, size, &read, NULL) && read > 0u;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, raw);
        return 0;
    }
    raw[read] = '\0';

    KboAsianGamesRosterEntry loaded[KBO_ASIAN_GAMES_ROSTER_SIZE];
    memset(loaded, 0, sizeof(loaded));
    int loaded_count = 0;
    uint32_t loaded_year = 0;

    char* line = raw;
    while (line != NULL && *line != '\0') {
        char* next = strchr(line, '\n');
        if (next != NULL) { *next++ = '\0'; }
        char* cr = strchr(line, '\r');
        if (cr != NULL) { *cr = '\0'; }

        if (strncmp(line, "year,", 5) != 0 && line[0] != '\0') {
            unsigned int year = 0, index = 0, player_id = 0, team_id = 0, league_id = 0;
            unsigned int departure_date = 0, return_date = 0, age = 0, role = 0, wildcard = 0;
            unsigned int old_restricted = 0, old_secondary = 0, old_injury = 0;
            int old_days = 0, score = 0;
            unsigned int departed = 0, returned = 0, exempted = 0;
            int fields = sscanf(
                line,
                "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d",
                &year, &index, &player_id, &team_id, &league_id, &departure_date, &return_date,
                &age, &role, &wildcard, &old_restricted, &old_secondary, &old_injury, &old_days,
                &departed, &returned, &exempted, &score);
            if (fields == 18 && player_id != 0u && loaded_count < KBO_ASIAN_GAMES_ROSTER_SIZE) {
                KboAsianGamesRosterEntry* entry = &loaded[loaded_count++];
                loaded_year = year;
                entry->player_id = player_id;
                entry->original_team_id = team_id;
                entry->original_league_id = league_id;
                entry->departure_date = departure_date;
                entry->return_date = return_date;
                entry->age = (uint16_t)age;
                entry->role = (uint8_t)role;
                entry->wildcard = (uint8_t)wildcard;
                entry->old_restricted = (uint8_t)old_restricted;
                entry->old_secondary_restricted = (uint8_t)old_secondary;
                entry->old_injury_active = (uint8_t)old_injury;
                entry->old_injury_days_left = (int16_t)old_days;
                entry->departed = (uint8_t)departed;
                entry->returned = (uint8_t)returned;
                entry->exempted = (uint8_t)exempted;
                entry->score = score;
                entry->player_ptr = 0;
            }
        }
        line = next;
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (loaded_count <= 0) {
        return 0;
    }

    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    memcpy(g_kbo_asian_games_roster, loaded, sizeof(KboAsianGamesRosterEntry) * (size_t)loaded_count);
    g_kbo_asian_games_roster_count = loaded_count;
    g_kbo_asian_games_roster_year = loaded_year;
    snprintf(g_kbo_asian_games_roster_save_path, sizeof(g_kbo_asian_games_roster_save_path), "%s", path);
    append_logf("KBO Asian Games roster csv load source=%s year=%u count=%d path=%s", source != NULL ? source : "", loaded_year, loaded_count, path);
    return loaded_count;
}


