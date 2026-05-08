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
