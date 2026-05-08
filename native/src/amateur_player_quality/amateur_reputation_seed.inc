static void kbo_amateur_reputation_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }
    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1u);
    }
    size_t len = strlen(value);
    while (len > 0u) {
        char c = value[len - 1u];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '"') {
            break;
        }
        value[--len] = '\0';
    }
}

static void kbo_amateur_reputation_read_cell(const char** cursor, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (cursor == NULL || *cursor == NULL) {
        return;
    }

    const char* p = *cursor;
    size_t len = 0u;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }
    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (len + 1u < out_size) {
                        out[len++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                if (*p == ',') {
                    p++;
                }
                break;
            }
        } else if (*p == ',') {
            p++;
            break;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }
        if (len + 1u < out_size) {
            out[len++] = *p;
        }
        p++;
    }
    out[len] = '\0';
    kbo_amateur_reputation_trim_cell(out);
    *cursor = p;
}

static uint32_t kbo_amateur_reputation_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    unsigned long value = strtoul(text, NULL, 10);
    if (value > UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)value;
}

static int kbo_get_reputation_seed_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\%s", local_app_data, file_name);
    return out[0] != '\0';
}

static int kbo_read_amateur_reputation_seed_file(const char* path, char** out_buffer, DWORD* out_size)
{
    if (out_buffer == NULL || out_size == NULL) {
        return 0;
    }
    *out_buffer = NULL;
    *out_size = 0;
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER file_size = {0};
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }

    DWORD size = (DWORD)file_size.QuadPart;
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int ok = ReadFile(file, buffer, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    buffer[size] = '\0';
    *out_buffer = buffer;
    *out_size = size;
    return 1;
}

static int kbo_amateur_reputation_add_seed(
    uint32_t league_id,
    uint32_t team_id,
    const char* team_abbr,
    const char* team_name,
    const char* nick_name,
    uint32_t reputation)
{
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    if (reputation < 1u) {
        reputation = 1u;
    } else if (reputation > 100u) {
        reputation = 100u;
    }

    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        if (g_kbo_amateur_reputation_seeds[i].league_id == league_id
                && g_kbo_amateur_reputation_seeds[i].team_id == team_id) {
            if (team_abbr != NULL && team_abbr[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].team_abbr, sizeof(g_kbo_amateur_reputation_seeds[i].team_abbr), "%s", team_abbr);
            }
            if (team_name != NULL && team_name[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].team_name, sizeof(g_kbo_amateur_reputation_seeds[i].team_name), "%s", team_name);
            }
            if (nick_name != NULL && nick_name[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].nick_name, sizeof(g_kbo_amateur_reputation_seeds[i].nick_name), "%s", nick_name);
            }
            g_kbo_amateur_reputation_seeds[i].reputation = (uint8_t)reputation;
            return 1;
        }
    }
    if (g_kbo_amateur_reputation_seed_count >= KBO_AMATEUR_REPUTATION_SEED_MAX) {
        return 0;
    }
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].league_id = league_id;
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_id = team_id;
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_abbr, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_abbr), "%s", team_abbr != NULL ? team_abbr : "");
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_name, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_name), "%s", team_name != NULL ? team_name : "");
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].nick_name, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].nick_name), "%s", nick_name != NULL ? nick_name : "");
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].reputation = (uint8_t)reputation;
    g_kbo_amateur_reputation_seed_count++;
    return 1;
}

static int kbo_load_reputation_seed_path_into_cache(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_amateur_reputation_seed_file(path, &buffer, &size)) {
        append_logf("team reputation seed load skipped path=%s reason=read_failed", path);
        return 0;
    }

    int before = g_kbo_amateur_reputation_seed_count;
    char* cursor = buffer;
    while (*cursor != '\0') {
        char* line = cursor;
        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor == '\n') {
            *cursor++ = '\0';
        }
        if (line[0] == '#' || line[0] == '\0' || strstr(line, "league_id,") == line) {
            continue;
        }

        const char* cell_cursor = line;
        char cell[128] = {0};
        uint32_t league_id = 0u;
        uint32_t team_id = 0u;
        char team_abbr[32] = {0};
        char team_name[96] = {0};
        char nick_name[64] = {0};
        uint32_t reputation = 0u;
        for (int col = 0; col <= 9; col++) {
            kbo_amateur_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            if (col == 0) {
                league_id = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 5) {
                team_id = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 6) {
                snprintf(team_abbr, sizeof(team_abbr), "%s", cell);
            } else if (col == 7) {
                snprintf(team_name, sizeof(team_name), "%s", cell);
            } else if (col == 8) {
                snprintf(nick_name, sizeof(nick_name), "%s", cell);
            } else if (col == 9) {
                reputation = kbo_amateur_reputation_parse_u32(cell);
            }
        }
        kbo_amateur_reputation_add_seed(league_id, team_id, team_abbr, team_name, nick_name, reputation);
    }

    int added = g_kbo_amateur_reputation_seed_count - before;
    HeapFree(GetProcessHeap(), 0, buffer);
    append_logf("team reputation seed loaded added=%d total=%d path=%s", added, g_kbo_amateur_reputation_seed_count, path);
    return 1;
}
