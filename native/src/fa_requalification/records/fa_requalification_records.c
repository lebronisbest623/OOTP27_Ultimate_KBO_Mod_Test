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
        "player_id,original_team_id,last_fa_year,fa_count,last_fa_grade\r\n"
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

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_records && kbo_csv_reader_next_row(reader)) {
        char fields[5][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 5);
        if (field_count < 4
                || fields[0][0] == '\0'
                || fields[0][0] == '#'
                || fields[0][0] == ';'
                || (fields[0][0] >= 'A' && fields[0][0] <= 'z')) {
            continue;
        }

        uint32_t player_id = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t original_team_id = kbo_csv_parse_u32_text(fields[1], 10);
        uint32_t last_fa_year = kbo_csv_parse_u32_text(fields[2], 10);
        uint32_t fa_count = kbo_csv_parse_u32_text(fields[3], 10);
        if (player_id != 0u
                && original_team_id != 0u
                && last_fa_year >= 1982u
                && last_fa_year <= 2200u
                && fa_count >= 1u) {
            records[count].player_id = player_id;
            records[count].original_team_id = original_team_id;
            records[count].last_fa_year = last_fa_year;
            records[count].fa_count = fa_count;
            snprintf(
                records[count].last_fa_grade,
                sizeof(records[count].last_fa_grade),
                "%.*s",
                (int)sizeof(records[count].last_fa_grade) - 1,
                field_count >= 5 && fields[4][0] != '\0' ? fields[4] : "UNKNOWN");
            count++;
        }
    }

    kbo_csv_reader_close(reader);
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
        "player_id,original_team_id,last_fa_year,fa_count,last_fa_grade\r\n"
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + configured team_control_years.\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < count; i++) {
        if (records[i].player_id == 0u || records[i].original_team_id == 0u || records[i].last_fa_year == 0u) {
            continue;
        }
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%s\r\n",
            records[i].player_id,
            records[i].original_team_id,
            records[i].last_fa_year,
            records[i].fa_count,
            records[i].last_fa_grade[0] != '\0' ? records[i].last_fa_grade : "UNKNOWN");
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

int kbo_record_fa_requalification_signing_with_grade(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t signing_year,
    const char* grade,
    const char* source)
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
        snprintf(records[found].last_fa_grade, sizeof(records[found].last_fa_grade), "%s", grade != NULL && grade[0] != '\0' ? grade : "UNKNOWN");
    } else {
        if (count >= KBO_FA_REQUALIFICATION_MAX) {
            append_logf("KBO FA requalification signing record dropped source=%s player=%u team=%u reason=max_records", source != NULL ? source : "", player_id, team_id);
            kbo_unlock_fa_requalification_records();
            return 0;
        }
        records[count].player_id = player_id;
        records[count].original_team_id = team_id;
        records[count].last_fa_year = signing_year;
        records[count].fa_count = 1u;
        snprintf(records[count].last_fa_grade, sizeof(records[count].last_fa_grade), "%s", grade != NULL && grade[0] != '\0' ? grade : "UNKNOWN");
        count++;
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

int kbo_record_fa_requalification_signing(uint32_t player_id, uint32_t team_id, uint32_t signing_year, const char* source)
{
    return kbo_record_fa_requalification_signing_with_grade(player_id, team_id, signing_year, "UNKNOWN", source);
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

