#include "../fa_requalification_internal.h"

int get_kbo_fa_requalification_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_requalification.csv", out, out_size);
}

void kbo_lock_fa_requalification_records(void)
{
    while (InterlockedCompareExchange(&g_kbo_fa_requalification_records_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_fa_requalification_records(void)
{
    InterlockedExchange(&g_kbo_fa_requalification_records_lock, 0);
}

int kbo_fa_parse_u32_csv_field(const char** cursor, uint32_t* out_value)
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

void kbo_ensure_fa_requalification_template(void)
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
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + configured team_control_years.\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    CloseHandle(file);
    append_logf("KBO FA requalification template created path=%s", path);
}

int kbo_load_fa_requalification_records(KboFaRequalificationRecord* records, int max_records)
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

int kbo_write_fa_requalification_records(const KboFaRequalificationRecord* records, int count)
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
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + configured team_control_years.\r\n";
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

int kbo_record_fa_requalification_signing(uint32_t player_id, uint32_t team_id, uint32_t signing_year, const char* source)
{
    if (player_id == 0u || team_id == 0u || signing_year < 1982u || signing_year > 2200u) {
        return 0;
    }

    kbo_lock_fa_requalification_records();
    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (records[i].player_id == player_id) {
            found = i;
            break;
        }
    }

    if (found >= 0) {
        if (records[found].original_team_id == team_id && records[found].last_fa_year == signing_year) {
            append_logf(
                "KBO FA requalification signing already recorded source=%s player=%u team=%u signing_year=%u fa_count=%u",
                source != NULL ? source : "",
                player_id,
                team_id,
                signing_year,
                records[found].fa_count);
            kbo_unlock_fa_requalification_records();
            return 1;
        }
        records[found].original_team_id = team_id;
        records[found].last_fa_year = signing_year;
        records[found].fa_count = records[found].fa_count + 1u;
        if (records[found].fa_count == 0u) {
            records[found].fa_count = 1u;
        }
    } else {
        if (count >= KBO_FA_REQUALIFICATION_MAX) {
            append_logf("KBO FA requalification signing record dropped source=%s player=%u team=%u reason=max_records", source != NULL ? source : "", player_id, team_id);
            kbo_unlock_fa_requalification_records();
            return 0;
        }
        records[count++] = (KboFaRequalificationRecord){
            player_id,
            team_id,
            signing_year,
            1u
        };
    }

    int ok = kbo_write_fa_requalification_records(records, count);
    append_logf(
        "KBO FA requalification signing recorded source=%s player=%u team=%u signing_year=%u eligible_year=%u records=%d ok=%d",
        source != NULL ? source : "",
        player_id,
        team_id,
        signing_year,
        signing_year + kbo_fa_requalification_team_control_years(),
        count,
        ok);
    kbo_unlock_fa_requalification_records();
    return ok;
}

int kbo_fa_requalification_team_ptr_is_kbo(
    uintptr_t team_ptr,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (out_team_id != NULL) {
        *out_team_id = team_id;
    }
    if (out_league_id != NULL) {
        *out_league_id = league_id;
    }
    if (team_id == 0u || team_id > 100000u || league_id == 0u || league_id > 100000u) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id == 0u || league_id == kbo_league_id;
}

