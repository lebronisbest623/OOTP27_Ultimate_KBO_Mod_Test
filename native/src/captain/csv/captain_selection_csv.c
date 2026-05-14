#include "../internal/captain_selection_internal.h"

int kbo_captain_selection_csv_path(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || season < 1982u || season > 2200u) {
        return 0;
    }

    char file_name[96] = {0};
    snprintf(file_name, sizeof(file_name), "captains_%04u.csv", season);
    return kbo_get_save_scoped_data_file(file_name, out, out_size);
}

int kbo_captain_selection_csv_exists(uint32_t season)
{
    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    return attrs.nFileSizeHigh != 0u || attrs.nFileSizeLow != 0u;
}

static void kbo_captain_trim_loaded_csv_token(char* text)
{
    if (text == NULL) {
        return;
    }

    char* start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }
}

static int kbo_captain_loaded_csv_parse_u32(const char* text, uint32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }

    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint64_t value = 0u;
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10u + (uint64_t)(*p - '0');
        if (value > 0xffffffffu) {
            return 0;
        }
    }

    *out = (uint32_t)value;
    return 1;
}

static int kbo_captain_loaded_csv_parse_i32(const char* text, int32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }

    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    int negative = 0;
    if (*p == '-') {
        negative = 1;
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    int64_t value = 0;
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10 + (int64_t)(*p - '0');
        if ((!negative && value > 2147483647)
                || (negative && value > 2147483648LL)) {
            return 0;
        }
    }

    *out = negative ? (int32_t)-value : (int32_t)value;
    return 1;
}

static int kbo_captain_read_loaded_csv_fields(
    const char* line,
    char fields[][160],
    int max_fields)
{
    if (line == NULL || fields == NULL || max_fields <= 0) {
        return 0;
    }

    int field = 0;
    int pos = 0;
    int in_quotes = 0;
    memset(fields, 0, (SIZE_T)max_fields * 160u);

    for (const char* p = line; *p != '\0' && *p != '\r' && *p != '\n'; p++) {
        char ch = *p;
        if (in_quotes) {
            if (ch == '"' && p[1] == '"') {
                if (pos < 159) {
                    fields[field][pos++] = '"';
                }
                p++;
            } else if (ch == '"') {
                in_quotes = 0;
            } else if (pos < 159) {
                fields[field][pos++] = ch;
            }
            continue;
        }

        if (ch == '"') {
            in_quotes = 1;
        } else if (ch == ',') {
            fields[field][pos] = '\0';
            kbo_captain_trim_loaded_csv_token(fields[field]);
            field++;
            if (field >= max_fields) {
                return max_fields;
            }
            pos = 0;
        } else if (pos < 159) {
            fields[field][pos++] = ch;
        }
    }

    fields[field][pos] = '\0';
    kbo_captain_trim_loaded_csv_token(fields[field]);
    return field + 1;
}

static void kbo_captain_copy_loaded_csv_text(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    const char* src = value != NULL ? value : "";
    size_t len = strlen(src);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, src, len);
    out[len] = '\0';
}

static int kbo_captain_parse_selection_csv_row(
    const char* line,
    uint32_t expected_season,
    KboCaptainSelectionRow* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }

    char fields[28][160];
    int count = kbo_captain_read_loaded_csv_fields(line, fields, 28);
    if (count < 8 || fields[0][0] == '\0' || _stricmp(fields[0], "date") == 0) {
        return 0;
    }

    KboCaptainSelectionRow row;
    memset(&row, 0, sizeof(row));
    uint32_t parsed_u32 = 0u;
    int32_t parsed_i32 = 0;

    if (kbo_captain_loaded_csv_parse_u32(fields[0], &parsed_u32)) { row.date = parsed_u32; }
    if (kbo_captain_loaded_csv_parse_u32(fields[1], &parsed_u32)) { row.season = parsed_u32; }
    if (row.season != expected_season) {
        return 0;
    }
    if (kbo_captain_loaded_csv_parse_u32(fields[3], &parsed_u32)) { row.league_id = parsed_u32; }
    if (kbo_captain_loaded_csv_parse_u32(fields[4], &parsed_u32)) { row.team_id = parsed_u32; }
    if (row.team_id == 0u) {
        return 0;
    }

    kbo_captain_copy_loaded_csv_text(row.team_name, sizeof(row.team_name), fields[5]);
    if (kbo_captain_loaded_csv_parse_u32(fields[6], &parsed_u32)) { row.player_id = parsed_u32; }
    kbo_captain_copy_loaded_csv_text(row.player_name, sizeof(row.player_name), fields[7]);
    if (count > 8 && kbo_captain_loaded_csv_parse_i32(fields[8], &parsed_i32)) { row.score = parsed_i32; }
    if (count > 9) { kbo_captain_copy_loaded_csv_text(row.reason, sizeof(row.reason), fields[9]); }
    if (count > 10 && kbo_captain_loaded_csv_parse_u32(fields[10], &parsed_u32)) { row.seeded = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 11 && kbo_captain_loaded_csv_parse_i32(fields[11], &parsed_i32)) { row.seed_priority = parsed_i32; }
    if (count > 12) { kbo_captain_copy_loaded_csv_text(row.seed_source, sizeof(row.seed_source), fields[12]); }
    if (count > 13 && kbo_captain_loaded_csv_parse_u32(fields[13], &parsed_u32)) { row.nation_id = parsed_u32; }
    if (count > 14 && kbo_captain_loaded_csv_parse_u32(fields[14], &parsed_u32)) { row.domestic = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 15 && kbo_captain_loaded_csv_parse_u32(fields[15], &parsed_u32)) { row.current_team_id = parsed_u32; }
    if (count > 16 && kbo_captain_loaded_csv_parse_u32(fields[16], &parsed_u32)) { row.active_team_id = parsed_u32; }
    if (count > 17 && kbo_captain_loaded_csv_parse_u32(fields[17], &parsed_u32)) { row.current_league_id = parsed_u32; }
    if (count > 18 && kbo_captain_loaded_csv_parse_u32(fields[18], &parsed_u32)) { row.age = (uint16_t)parsed_u32; }
    if (count > 19 && kbo_captain_loaded_csv_parse_i32(fields[19], &parsed_i32)) { row.salary = parsed_i32; }
    if (count > 20 && kbo_captain_loaded_csv_parse_i32(fields[20], &parsed_i32)) { row.value_score = parsed_i32; }
    if (count > 21 && kbo_captain_loaded_csv_parse_i32(fields[21], &parsed_i32)) { row.overall_value = (int16_t)parsed_i32; }
    if (count > 22 && kbo_captain_loaded_csv_parse_i32(fields[22], &parsed_i32)) { row.talent_value = (int16_t)parsed_i32; }
    if (count > 23 && kbo_captain_loaded_csv_parse_i32(fields[23], &parsed_i32)) { row.ratings_value = (int16_t)parsed_i32; }
    if (count > 24 && kbo_captain_loaded_csv_parse_i32(fields[24], &parsed_i32)) { row.career_value = (int16_t)parsed_i32; }
    if (count > 25 && kbo_captain_loaded_csv_parse_u32(fields[25], &parsed_u32)) { row.dfa = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 26 && kbo_captain_loaded_csv_parse_u32(fields[26], &parsed_u32)) { row.restricted = parsed_u32 != 0u ? 1u : 0u; }
    if (count > 27 && kbo_captain_loaded_csv_parse_u32(fields[27], &parsed_u32)) { row.injured = parsed_u32 != 0u ? 1u : 0u; }

    *out = row;
    return 1;
}

int kbo_captain_load_selection_csv(
    uint32_t season,
    KboCaptainSelectionRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0 || season < 1982u || season > 2200u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 131072u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int count = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0' && count < max_rows) {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[2048] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            KboCaptainSelectionRow row;
            if (kbo_captain_parse_selection_csv_row(line, season, &row)) {
                rows[count++] = row;
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    return count;
}

static void kbo_captain_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

static int kbo_captain_write_header(HANDLE file)
{
    const char* header =
        "date,season,source,league_id,team_id,team_name,captain_player_id,captain_name,"
        "score,reason,seeded,seed_priority,seed_source,nation_id,domestic,current_team_id,active_team_id,current_league_id,"
        "age,salary,value_score,overall_value,talent_value,ratings_value,career_value,"
        "dfa,restricted,injured\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

int kbo_captain_write_selection_csv(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || row_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(rows[0].season, path, sizeof(path))) {
        append_logf(
            "KBO captain selection csv skipped source=%s reason=path_unavailable season=%u",
            source != NULL ? source : "",
            rows[0].season);
        return 0;
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
        append_logf("KBO captain selection csv open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    int ok = kbo_captain_write_header(file);
    DWORD written = 0;
    for (int i = 0; ok && i < row_count; i++) {
        const KboCaptainSelectionRow* row = &rows[i];
        char text[512] = {0};
        int len = snprintf(
            text,
            sizeof(text),
            "%u,%u,",
            row->date,
            row->season);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, source != NULL ? source : "");
        len = snprintf(
            text,
            sizeof(text),
            ",%u,%u,",
            row->league_id,
            row->team_id);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->team_name);
        len = snprintf(text, sizeof(text), ",%u,", row->player_id);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->player_name);
        len = snprintf(
            text,
            sizeof(text),
            ",%d,",
            row->score);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->reason);
        len = snprintf(
            text,
            sizeof(text),
            ",%u,%d,",
            (uint32_t)row->seeded,
            row->seed_priority);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->seed_source);
        len = snprintf(
            text,
            sizeof(text),
            ",%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%u,%u,%u\r\n",
            row->nation_id,
            (uint32_t)row->domestic,
            row->current_team_id,
            row->active_team_id,
            row->current_league_id,
            (uint32_t)row->age,
            row->salary,
            row->value_score,
            (int)row->overall_value,
            (int)row->talent_value,
            (int)row->ratings_value,
            (int)row->career_value,
            (uint32_t)row->dfa,
            (uint32_t)row->restricted,
            (uint32_t)row->injured);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
        }
    }

    CloseHandle(file);
    if (!ok) {
        append_logf("KBO captain selection csv write failed path=%s", path);
        return 0;
    }

    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}
