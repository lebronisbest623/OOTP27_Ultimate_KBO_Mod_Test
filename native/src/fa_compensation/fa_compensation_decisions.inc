static int kbo_load_fa_compensation_protection_debug_rows(
    KboFaCompensationProtectionDebugRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protection_debug_path(path, sizeof(path))) {
        return 0;
    }
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 2u * 1024u * 1024u) {
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
            char fields[16][128];
            memset(fields, 0, sizeof(fields));
            int ok = 1;
            for (int field_index = 0; field_index < 16; field_index++) {
                if (!kbo_fa_market_parse_csv_field(&p, fields[field_index], sizeof(fields[field_index]))) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                KboFaCompensationProtectionDebugRow* row = &rows[count];
                row->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
                row->season = kbo_fa_compensation_parse_u32(fields[1]);
                kbo_fa_compensation_copy_token(fields[2], row->grade, sizeof(row->grade));
                row->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
                row->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                row->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
                row->generated_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
                kbo_fa_compensation_copy_token(fields[7], row->list_type, sizeof(row->list_type));
                row->rank = kbo_fa_compensation_parse_u32(fields[8]);
                row->player_id = kbo_fa_compensation_parse_u32(fields[9]);
                kbo_fa_compensation_copy_token(fields[10], row->player_name, sizeof(row->player_name));
                row->score = kbo_fa_compensation_parse_i32(fields[11]);
                row->age = (uint16_t)kbo_fa_compensation_parse_u32(fields[12]);
                row->role = (uint8_t)kbo_fa_compensation_parse_u32(fields[13]);
                kbo_fa_compensation_copy_token(fields[14], row->reason, sizeof(row->reason));
                kbo_fa_compensation_copy_token(fields[15], row->source, sizeof(row->source));
                if (row->fa_player_id != 0u && row->player_id != 0u) {
                    count++;
                } else {
                    memset(row, 0, sizeof(*row));
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

static int kbo_persist_fa_compensation_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation decision persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,signed_on,due_on,decided_on,"
            "action,selected_player_id,selected_player_name,selected_player_score,unprotected_candidate_count,"
            "cash_with_player,cash_only,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char prefix[256] = {0};
    int len = snprintf(
        prefix,
        sizeof(prefix),
        "%u,%u,%s,%u,%u,%u,%u,%u,PLAYER,%u,",
        rec->player_id,
        rec->season,
        rec->grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        decided_yyyymmdd,
        selected->player_id);
    if (len > 0 && len < (int)sizeof(prefix)) {
        WriteFile(file, prefix, (DWORD)len, &written, NULL);
    }
    kbo_fa_market_write_csv_text(file, selected->player_name);

    char suffix[160] = {0};
    len = snprintf(
        suffix,
        sizeof(suffix),
        ",%d,%d,%u,%u,",
        selected->score,
        unprotected_candidate_count,
        rec->cash_with_player,
        rec->cash_only);
    if (len > 0 && len < (int)sizeof(suffix)) {
        WriteFile(file, suffix, (DWORD)len, &written, NULL);
    }
    kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_player_ai");
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);

    append_logf(
        "KBO FA compensation player selected fa_player=%u original_team=%u signing_team=%u selected=%u name=%s score=%d unprotected=%d decision=%s",
        rec->player_id,
        rec->original_team_id,
        rec->signing_team_id,
        selected->player_id,
        selected->player_name,
        selected->score,
        unprotected_candidate_count,
        path);
    return 1;
}

static int kbo_persist_fa_compensation_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->cash_only == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation cash-only decision persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,signed_on,due_on,decided_on,"
            "action,selected_player_id,selected_player_name,selected_player_score,unprotected_candidate_count,"
            "cash_with_player,cash_only,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char row[512] = {0};
    int len = snprintf(
        row,
        sizeof(row),
        "%u,%u,%s,%u,%u,%u,%u,%u,CASH_ONLY,0,\"\",0,0,%u,%u,",
        rec->player_id,
        rec->season,
        rec->grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        decided_yyyymmdd,
        rec->cash_with_player,
        rec->cash_only);
    if (len > 0 && len < (int)sizeof(row)) {
        WriteFile(file, row, (DWORD)len, &written, NULL);
    }
    kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_cash_only_ai");
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);

    append_logf(
        "KBO FA compensation cash-only selected fa_player=%u original_team=%u signing_team=%u cash_only=%u decision=%s",
        rec->player_id,
        rec->original_team_id,
        rec->signing_team_id,
        rec->cash_only,
        path);
    return 1;
}

static int kbo_load_latest_fa_compensation_decision(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (fa_player_id == 0u || out == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
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

    int found = 0;
    char* cursor = buffer;
    while (*cursor != '\0') {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }
        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            char fields[16][128];
            memset(fields, 0, sizeof(fields));
            int ok = 1;
            for (int field_index = 0; field_index < 16; field_index++) {
                if (!kbo_fa_market_parse_csv_field(&p, fields[field_index], sizeof(fields[field_index]))) {
                    ok = 0;
                    break;
                }
            }
            if (ok && kbo_fa_compensation_parse_u32(fields[0]) == fa_player_id) {
                out->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
                out->season = kbo_fa_compensation_parse_u32(fields[1]);
                kbo_fa_compensation_copy_token(fields[2], out->grade, sizeof(out->grade));
                out->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
                out->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                out->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
                out->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
                out->decided_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[7]);
                kbo_fa_compensation_copy_token(fields[8], out->action, sizeof(out->action));
                out->selected_player_id = kbo_fa_compensation_parse_u32(fields[9]);
                kbo_fa_compensation_copy_token(fields[10], out->selected_player_name, sizeof(out->selected_player_name));
                out->selected_player_score = kbo_fa_compensation_parse_i32(fields[11]);
                out->unprotected_candidate_count = (int)kbo_fa_compensation_parse_u32(fields[12]);
                out->cash_with_player = kbo_fa_compensation_parse_u32(fields[13]);
                out->cash_only = kbo_fa_compensation_parse_u32(fields[14]);
                kbo_fa_compensation_copy_token(fields[15], out->source, sizeof(out->source));
                found = out->selected_player_id != 0u || strcmp(out->action, "CASH_ONLY") == 0;
            }
        }
        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}
