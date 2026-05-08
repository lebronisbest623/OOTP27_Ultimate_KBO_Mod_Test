/* Save-scoped FA filing ledger captured from live OOTP transitions. */

#define KBO_FA_FILING_MAX 4096

typedef struct KboFaFilingRecord {
    uint32_t player_id;
    uint32_t filing_date;
    uint32_t season;
    uint32_t original_team_id;
    uint32_t league_id;
    uint32_t source_caller_rva;
    uint8_t notify;
    uint8_t contract_level;
    char player_name[96];
    char source[48];
} KboFaFilingRecord;

static volatile LONG g_kbo_fa_filing_lock = 0;
static volatile LONG g_kbo_fa_filing_cache_dirty = 1;
static KboFaFilingRecord* g_kbo_fa_filing_cache_rows = NULL;
static int g_kbo_fa_filing_cache_count = 0;
static char g_kbo_fa_filing_cache_path[MAX_PATH] = {0};

#define KBO_FA_FILING_NEGATIVE_CACHE_MAX 512
static uint32_t g_kbo_fa_filing_negative_cache[KBO_FA_FILING_NEGATIVE_CACHE_MAX];
static volatile LONG g_kbo_fa_filing_negative_cache_cursor = 0;

static int kbo_fa_filing_negative_cache_contains(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < KBO_FA_FILING_NEGATIVE_CACHE_MAX; i++) {
        if (g_kbo_fa_filing_negative_cache[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_fa_filing_negative_cache_add(uint32_t player_id)
{
    if (player_id == 0u) {
        return;
    }
    LONG slot = InterlockedIncrement(&g_kbo_fa_filing_negative_cache_cursor);
    g_kbo_fa_filing_negative_cache[(uint32_t)slot % KBO_FA_FILING_NEGATIVE_CACHE_MAX] = player_id;
}

static void kbo_fa_filing_negative_cache_clear(void)
{
    memset(g_kbo_fa_filing_negative_cache, 0, sizeof(g_kbo_fa_filing_negative_cache));
    InterlockedExchange(&g_kbo_fa_filing_negative_cache_cursor, 0);
}

static int kbo_get_fa_filing_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_filing.csv", out, out_size);
}

static void kbo_fa_filing_enter_lock(void)
{
    while (InterlockedCompareExchange(&g_kbo_fa_filing_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_fa_filing_leave_lock(void)
{
    InterlockedExchange(&g_kbo_fa_filing_lock, 0);
}

static uint32_t kbo_fa_filing_parse_u32(const char* text)
{
    if (text == NULL) {
        return 0u;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    if (*text == '\0') {
        return 0u;
    }
    char* tail = NULL;
    unsigned long value = strtoul(text, &tail, 0);
    if (tail == text) {
        return 0u;
    }
    return (uint32_t)value;
}

static void kbo_fa_filing_copy_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }
    size_t used = 0;
    while (text[used] != '\0' && used + 1u < out_size) {
        out[used] = text[used];
        used++;
    }
    out[used] = '\0';
}

static int kbo_fa_filing_parse_csv_field(char** cursor, char* out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }

    size_t used = 0;
    if (*p == '"') {
        p++;
        while (*p != '\0') {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (used + 1u < out_size) {
                        out[used++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                break;
            }
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (*p != '\0' && *p != ',' && *p != '\n') {
            p++;
        }
    } else {
        while (*p != '\0' && *p != ',' && *p != '\n') {
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (used > 0u && (out[used - 1u] == ' ' || out[used - 1u] == '\t' || out[used - 1u] == '\r')) {
            used--;
        }
    }

    out[used] = '\0';
    if (*p == ',') {
        p++;
    }
    *cursor = p;
    return 1;
}

static void kbo_fa_filing_write_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

static void kbo_fa_filing_write_csv_text(HANDLE file, const char* text)
{
    kbo_fa_filing_write_raw(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                kbo_fa_filing_write_raw(file, "\"\"");
            } else {
                char one[2] = {*p, '\0'};
                kbo_fa_filing_write_raw(file, one);
            }
        }
    }
    kbo_fa_filing_write_raw(file, "\"");
}

static int kbo_load_fa_filing_records_unlocked(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_filing_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 4u * 1024u * 1024u) {
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
    char* cursor = buffer;
    while (*cursor != '\0' && count < max_rows) {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }

        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            char fields[10][128];
            memset(fields, 0, sizeof(fields));
            int field_count = 0;
            for (; field_count < 10 && *p != '\0'; field_count++) {
                if (!kbo_fa_filing_parse_csv_field(&p, fields[field_count], sizeof(fields[field_count]))) {
                    break;
                }
            }

            if (field_count >= 8) {
                KboFaFilingRecord row;
                memset(&row, 0, sizeof(row));
                row.player_id = kbo_fa_filing_parse_u32(fields[0]);
                row.filing_date = kbo_fa_filing_parse_u32(fields[1]);
                row.season = kbo_fa_filing_parse_u32(fields[2]);
                row.original_team_id = kbo_fa_filing_parse_u32(fields[3]);
                row.league_id = kbo_fa_filing_parse_u32(fields[4]);
                row.source_caller_rva = kbo_fa_filing_parse_u32(fields[5]);
                row.notify = (uint8_t)(kbo_fa_filing_parse_u32(fields[6]) & 0xffu);
                row.contract_level = (uint8_t)(kbo_fa_filing_parse_u32(fields[7]) & 0xffu);
                if (field_count >= 9) {
                    kbo_fa_filing_copy_text(row.player_name, sizeof(row.player_name), fields[8]);
                }
                if (field_count >= 10) {
                    kbo_fa_filing_copy_text(row.source, sizeof(row.source), fields[9]);
                }
                if (row.player_id != 0u && row.filing_date != 0u && row.season != 0u) {
                    rows[count++] = row;
                }
            }
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

static int kbo_write_fa_filing_records_unlocked(
    const KboFaFilingRecord* rows,
    int row_count,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || row_count < 0 || row_count > KBO_FA_FILING_MAX) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_filing_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA filing csv open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    kbo_fa_filing_write_raw(
        file,
        "player_id,filing_date,season,original_team_id,league_id,source_caller_rva,notify,contract_level,name,source\r\n");

    for (int i = 0; i < row_count; i++) {
        const KboFaFilingRecord* row = &rows[i];
        char prefix[192] = {0};
        snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,%u,%u,%u,%u,%u,",
            row->player_id,
            row->filing_date,
            row->season,
            row->original_team_id,
            row->league_id,
            row->source_caller_rva,
            (uint32_t)row->notify,
            (uint32_t)row->contract_level);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, row->player_name);
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, row->source);
        kbo_fa_filing_write_raw(file, "\r\n");
    }

    CloseHandle(file);
    return 1;
}

static int kbo_load_fa_filing_records(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    kbo_fa_filing_enter_lock();
    int count = kbo_load_fa_filing_records_unlocked(rows, max_rows, out_path, out_path_size);
    kbo_fa_filing_leave_lock();
    return count;
}

static int kbo_fa_filing_find_latest_player(
    uint32_t player_id,
    uint32_t* out_original_team_id,
    uint32_t* out_league_id,
    uint32_t* out_season)
{
    if (out_original_team_id != NULL) {
        *out_original_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (out_season != NULL) {
        *out_season = 0u;
    }
    if (player_id == 0u) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_fa_filing_find_latest);

    char current_path[MAX_PATH] = {0};
    if (!kbo_get_fa_filing_csv_path(current_path, sizeof(current_path))) {
        KBO_PROFILE_END(profile_fa_filing_find_latest, "fa_filing.find_latest.no_path");
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_fa_filing_cache_dirty, 0, 0) == 0
            && strcmp(g_kbo_fa_filing_cache_path, current_path) == 0
            && kbo_fa_filing_negative_cache_contains(player_id)) {
        KBO_PROFILE_END(profile_fa_filing_find_latest, "fa_filing.find_latest.negative_cache");
        return 0;
    }

    kbo_fa_filing_enter_lock();
    int needs_reload = g_kbo_fa_filing_cache_rows == NULL
        || InterlockedCompareExchange(&g_kbo_fa_filing_cache_dirty, 0, 0) != 0
        || strcmp(g_kbo_fa_filing_cache_path, current_path) != 0;
    if (needs_reload) {
        if (g_kbo_fa_filing_cache_rows == NULL) {
            g_kbo_fa_filing_cache_rows = (KboFaFilingRecord*)HeapAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
        }
        if (g_kbo_fa_filing_cache_rows == NULL) {
            kbo_fa_filing_leave_lock();
            KBO_PROFILE_END(profile_fa_filing_find_latest, "fa_filing.find_latest.alloc_failed");
            return 0;
        }
        memset(g_kbo_fa_filing_cache_rows, 0, (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
        g_kbo_fa_filing_cache_count = kbo_load_fa_filing_records_unlocked(
            g_kbo_fa_filing_cache_rows,
            KBO_FA_FILING_MAX,
            g_kbo_fa_filing_cache_path,
            sizeof(g_kbo_fa_filing_cache_path));
        kbo_fa_filing_negative_cache_clear();
        InterlockedExchange(&g_kbo_fa_filing_cache_dirty, 0);
    }

    KboFaFilingRecord* rows = g_kbo_fa_filing_cache_rows;
    int row_count = g_kbo_fa_filing_cache_count;
    int best = -1;
    uint32_t best_date = 0u;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id != player_id || rows[i].filing_date == 0u) {
            continue;
        }
        if (best < 0 || rows[i].filing_date >= best_date) {
            best = i;
            best_date = rows[i].filing_date;
        }
    }

    if (best >= 0) {
        if (out_original_team_id != NULL) {
            *out_original_team_id = rows[best].original_team_id;
        }
        if (out_league_id != NULL) {
            *out_league_id = rows[best].league_id;
        }
        if (out_season != NULL) {
            *out_season = rows[best].season;
        }
    }

    kbo_fa_filing_leave_lock();
    if (best < 0) {
        kbo_fa_filing_negative_cache_add(player_id);
    }
    KBO_PROFILE_END(profile_fa_filing_find_latest, best >= 0 ? "fa_filing.find_latest.hit" : "fa_filing.find_latest.miss");
    return best >= 0 ? 1 : 0;
}

static int kbo_fa_filing_is_official_transition_caller(uintptr_t caller_rva)
{
    return caller_rva == 0x0067D11Eu;
}

static uint32_t kbo_fa_filing_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static int kbo_record_fa_filing_transition(
    uintptr_t player_ptr,
    uint32_t player_id,
    uint32_t filing_date,
    uint32_t original_team_id,
    uint32_t league_id,
    uint32_t caller_rva,
    uint8_t notify,
    uint8_t contract_level,
    const char* source)
{
    if (player_id == 0u || filing_date == 0u || original_team_id == 0u || league_id == 0u) {
        return 0;
    }

    uint32_t season = filing_date / 10000u;
    if (season < 1982u || season > 2200u) {
        return 0;
    }

    KboFaFilingRecord* rows = (KboFaFilingRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
    if (rows == NULL) {
        append_log_line("KBO FA filing record skipped: allocation failed");
        return 0;
    }

    char path[MAX_PATH] = {0};
    kbo_fa_filing_enter_lock();
    int row_count = kbo_load_fa_filing_records_unlocked(rows, KBO_FA_FILING_MAX, path, sizeof(path));
    int slot = -1;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id == player_id && rows[i].season == season) {
            slot = i;
            break;
        }
    }
    if (slot < 0 && row_count < KBO_FA_FILING_MAX) {
        slot = row_count++;
    }
    if (slot < 0) {
        kbo_fa_filing_leave_lock();
        HeapFree(GetProcessHeap(), 0, rows);
        append_logf("KBO FA filing record skipped: ledger full player=%u season=%u", player_id, season);
        return 0;
    }

    KboFaFilingRecord* rec = &rows[slot];
    memset(rec, 0, sizeof(*rec));
    rec->player_id = player_id;
    rec->filing_date = filing_date;
    rec->season = season;
    rec->original_team_id = original_team_id;
    rec->league_id = league_id;
    rec->source_caller_rva = caller_rva;
    rec->notify = notify;
    rec->contract_level = contract_level;
    if (player_ptr != 0 && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, rec->player_name, sizeof(rec->player_name));
    }
    if (rec->player_name[0] == '\0' || strcmp(rec->player_name, "Unknown player") == 0) {
        snprintf(rec->player_name, sizeof(rec->player_name), "Player #%u", player_id);
    }
    snprintf(rec->source, sizeof(rec->source), "%s", source != NULL ? source : "fa_filing_transition");

    int wrote = kbo_write_fa_filing_records_unlocked(rows, row_count, path, sizeof(path));
    if (wrote) {
        InterlockedExchange(&g_kbo_fa_filing_cache_dirty, 1);
        kbo_fa_filing_negative_cache_clear();
    }
    kbo_fa_filing_leave_lock();
    HeapFree(GetProcessHeap(), 0, rows);

    if (wrote) {
        append_logf(
            "KBO FA filing recorded player=%u date=%u season=%u original_team=%u league=%u caller_rva=0x%x csv=%s",
            player_id,
            filing_date,
            season,
            original_team_id,
            league_id,
            caller_rva,
            path);
        return 1;
    }
    append_logf("KBO FA filing record write failed player=%u date=%u csv=%s", player_id, filing_date, path);
    return 0;
}
