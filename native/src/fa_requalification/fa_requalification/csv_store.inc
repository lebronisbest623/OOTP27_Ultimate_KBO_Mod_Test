static int kbo_fa_parse_u32_csv_field(const char** cursor, uint32_t* out_value)
{
    if (cursor == NULL || *cursor == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    if (*p == '"') {
        p++;
    }

    char* tail = NULL;
    unsigned long value = strtoul(p, &tail, 10);
    if (tail == p || value > 0xfffffffful) {
        return 0;
    }

    while (*tail != '\0' && *tail != ',' && *tail != '\n') {
        tail++;
    }
    if (*tail == ',') {
        tail++;
    }

    *cursor = tail;
    *out_value = (uint32_t)value;
    return 1;
}

static void kbo_ensure_fa_requalification_template(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_fa_requalification_path(path, sizeof(path))) {
        return;
    }

    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const char* header =
        "player_id,original_team_id,last_fa_year,fa_count\r\n"
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + 4.\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    CloseHandle(file);
    append_logf("KBO FA requalification template created path=%s", path);
}

static int kbo_load_fa_requalification_records(KboFaRequalificationRecord* records, int max_records)
{
    if (records == NULL || max_records <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max_records * sizeof(records[0]));
    kbo_ensure_fa_requalification_template();

    char path[MAX_PATH] = {0};
    if (!get_kbo_fa_requalification_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    if (!ReadFile(file, buffer, size, &read, NULL)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    buffer[read] = '\0';

    int count = 0;
    const char* cursor = buffer;
    while (*cursor != '\0' && count < max_records) {
        const char* next = strchr(cursor, '\n');
        size_t len = next != NULL ? (size_t)(next - cursor) : strlen(cursor);
        if (len > 0 && len < 256) {
            char line[256] = {0};
            memcpy(line, cursor, len);
            const char* p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
            if (*p != '\0' && *p != '#' && *p != ';' && (*p < 'A' || *p > 'z')) {
                uint32_t player_id = 0;
                uint32_t original_team_id = 0;
                uint32_t last_fa_year = 0;
                uint32_t fa_count = 0;
                if (kbo_fa_parse_u32_csv_field(&p, &player_id)
                        && kbo_fa_parse_u32_csv_field(&p, &original_team_id)
                        && kbo_fa_parse_u32_csv_field(&p, &last_fa_year)
                        && kbo_fa_parse_u32_csv_field(&p, &fa_count)
                        && player_id != 0u
                        && original_team_id != 0u
                        && last_fa_year >= 1982u
                        && last_fa_year <= 2200u
                        && fa_count >= 1u) {
                    records[count++] = (KboFaRequalificationRecord){
                        player_id,
                        original_team_id,
                        last_fa_year,
                        fa_count
                    };
                }
            }
        }
        if (next == NULL || *next == '\0') {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

static int kbo_write_fa_requalification_records(const KboFaRequalificationRecord* records, int count)
{
    if (records == NULL || count < 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_fa_requalification_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA requalification write failed path=%s err=%lu", path, GetLastError());
        return 0;
    }

    char line[256] = {0};
    DWORD written = 0;
    const char* header =
        "player_id,original_team_id,last_fa_year,fa_count\r\n"
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + 4.\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < count; i++) {
        if (records[i].player_id == 0u || records[i].original_team_id == 0u || records[i].last_fa_year == 0u) {
            continue;
        }
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u\r\n",
            records[i].player_id,
            records[i].original_team_id,
            records[i].last_fa_year,
            records[i].fa_count);
        if (len > 0) {
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        append_logf("KBO FA requalification write: atomic commit failed path=%s", path);
        return 0;
    }
    return 1;
}
