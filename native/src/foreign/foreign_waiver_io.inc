/* Foreign waiver command and candidate CSV I/O. Included from native/src/foreign_waiver_ai.inc. */

static int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_commands.txt", out, out_size);
}

static int get_kbo_foreign_waiver_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_candidates.csv", out, out_size);
}

static int append_foreign_waiver_candidate_csv_header(HANDLE file)
{
    const char* header = "date,source,player_id,current_team_id,active_team_id,original_team_id,current_league_id,priority_window_open,priority_team_id,priority_eligible,dfa_flag,restricted,secondary_restricted,loan_active,injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}


static int kbo_append_foreign_waiver_cmd_line(const char* line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_cmd_path(path, sizeof(path))) {
        return 0;
    }
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_decision_lock, 1, 0) != 0) {
        Sleep(0);
    }
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
        return 0;
    }
    DWORD wrote = 0;
    char out[128] = {0};
    int len = snprintf(out, sizeof(out), "%s\r\n", line);
    WriteFile(file, out, (DWORD)len, &wrote, NULL);
    CloseHandle(file);
    InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
    return wrote == (DWORD)len;
}

static int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        append_logf("foreign waiver decision: blocked by window state team=%u player=%u action=%s", team_id, player_id, retain ? "RETAIN" : "SKIP");
        return 0;
    }
    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%s,%u,%u", retain ? "RETAIN" : "SKIP", team_id, player_id);
    if (len <= 0) {
        return 0;
    }
    return kbo_append_foreign_waiver_cmd_line(line);
}


static void process_foreign_waiver_commands(void)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_cmd_path(path, sizeof(path))) {
        append_log_line("foreign waiver command: unable to resolve command path");
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD file_size_low = GetFileSize(file, NULL);
    if (file_size_low == INVALID_FILE_SIZE || file_size_low == 0) {
        CloseHandle(file);
        return;
    }
    if (file_size_low >= 30000) {
        CloseHandle(file);
        append_log_line("foreign waiver command: command file too large; skip to avoid blocking");
        return;
    }

    char input[30000] = {0};
    DWORD read = 0;
    if (!ReadFile(file, input, file_size_low, &read, NULL) || read == 0) {
        CloseHandle(file);
        return;
    }
    CloseHandle(file);
    input[read] = '\0';

    char remaining[30000] = {0};
    char* keep = remaining;
    DWORD remain_len = 0;
    DWORD used_commands = 0;
    DWORD executed_commands = 0;

    const char* cursor = input;
    int line_no = 1;
    while (cursor < input + read) {
        const char* line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = input + read;
        }

        size_t len = (size_t)(line_end - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }

        char line[512] = {0};
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }
        memcpy(line, cursor, len);
        line[len] = '\0';

        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }

        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            if (remain_len + len + 1 < sizeof(remaining)) {
                size_t add_len = strlen(line);
                memcpy(keep + remain_len, line, add_len);
                keep[remain_len + add_len] = '\r';
                keep[remain_len + add_len + 1] = '\n';
                remain_len += (DWORD)(add_len + 2);
            }
        } else {
            used_commands++;
            if (!kbo_execute_foreign_waiver_claim(trimmed, line_no)) {
                if (remain_len + len + 2 < sizeof(remaining)) {
                    size_t add_len = strlen(trimmed);
                    memcpy(keep + remain_len, trimmed, add_len);
                    keep[remain_len + add_len] = '\r';
                    keep[remain_len + add_len + 1] = '\n';
                    remain_len += (DWORD)(add_len + 2);
                }
            } else {
                executed_commands++;
            }
        }

        if (*line_end == '\0') {
            break;
        }
        cursor = line_end + 1;
        line_no++;
    }

    if (used_commands > 0) {
        file = CreateFileA(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            if (remain_len > 0) {
                DWORD written = 0;
                WriteFile(file, remaining, remain_len, &written, NULL);
            }
            CloseHandle(file);
        }
        append_logf("foreign waiver command: processed=%lu executed=%lu keep_len=%lu", used_commands, executed_commands, remain_len);
        return;
    }
}


static int is_csv_empty(HANDLE file)
{
    DWORD high = 0;
    uint32_t low = GetFileSize(file, &high);
    if (high != 0) {
        return 0;
    }
    return low == 0;
}


static void write_foreign_waiver_candidates(const char* source)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            append_log_line("foreign waiver scanner: no player vector");
        }
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_csv_path(path, sizeof(path))) {
        append_log_line("foreign waiver scanner: unable to resolve output path");
        return;
    }

    {
        char dir[MAX_PATH] = {0};
        size_t len = strlen(path);
        if (len > 0 && len < sizeof(dir)) {
            strcpy_s(dir, sizeof(dir), path);
            char* slash = strrchr(dir, '\\');
            if (slash != NULL) {
                *slash = '\0';
                CreateDirectoryA(dir, NULL);
            }
        }
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign waiver scanner: failed to open %s", path);
        return;
    }

    if (is_csv_empty(file)) {
        append_foreign_waiver_candidate_csv_header(file);
    }
    SetFilePointer(file, 0, NULL, FILE_END);

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    int priority_window_open = kbo_is_foreign_waiver_negotiation_window_open();

    int scanned = 0;
    int written_candidates = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        uint8_t loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

        int forced_foreign = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        if (!forced_foreign && score <= 0) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t original_team_id = kbo_get_player_original_team_id(player);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        uint32_t priority_team_id = priority_window_open ? decision_team_id : 0;
        int priority_eligible = priority_window_open && decision_team_id != 0u;

        char line[320] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            date,
            (source == NULL ? "" : source),
            player_id,
            current_team_id,
            active_team_id,
            original_team_id,
            current_league_id,
            (uint32_t)priority_window_open,
            priority_team_id,
            (uint32_t)priority_eligible,
            (uint32_t)dfa,
            (uint32_t)restricted,
            (uint32_t)secondary,
            (uint32_t)loan_active,
            (uint32_t)inj_active,
            score);

        DWORD written = 0;
        WriteFile(file, line, (DWORD)len, &written, NULL);
        written_candidates++;
    }

    CloseHandle(file);

    append_logf("foreign waiver scanner: scanned=%d candidates=%d file=%s", scanned, written_candidates, path);
}

