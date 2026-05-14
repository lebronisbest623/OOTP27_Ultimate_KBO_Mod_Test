#include "../internal/foreign_injury_internal.h"
#include "../../../core/dates/core_text_date.h"

static int kbo_foreign_injury_persisted_date_span_plausible(
    uint32_t opened_on,
    uint32_t expected_end)
{
    if (opened_on == 0u || expected_end == 0u) {
        return 1;
    }

    uint32_t open_serial = kbo_date_serial(
        opened_on / 10000u,
        (opened_on / 100u) % 100u,
        opened_on % 100u);
    uint32_t end_serial = kbo_date_serial(
        expected_end / 10000u,
        (expected_end / 100u) % 100u,
        expected_end % 100u);
    if (open_serial == 0u || end_serial == 0u || end_serial < open_serial) {
        return 0;
    }

    return end_serial - open_serial <= 730u;
}

int kbo_foreign_injury_replacement_enabled(void)
{
    return kbo_fix_enabled() && !read_kbo_localappdata_flag_file("disable_foreign_injury_replacement.txt");
}

int kbo_foreign_injury_status_uses_slot(uint8_t status)
{
    return status == KBO_FOREIGN_INJURY_STATUS_OPEN || status == KBO_FOREIGN_INJURY_STATUS_ACTIVE;
}

uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player)
{
    return kbo_player_is_asian_quota_candidate(player)
        ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        : KBO_FOREIGN_INJURY_SLOT_REGULAR;
}

void kbo_lock_foreign_injury_replacements(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_foreign_injury_replacements(void)
{
    InterlockedExchange(&g_kbo_foreign_injury_replacement_lock, 0);
}

int kbo_load_foreign_injury_replacements_locked(const char* path)
{
    g_kbo_foreign_injury_replacement_count = 0;
    if (path == NULL || path[0] == '\0') {
        g_kbo_foreign_injury_replacement_loaded_path[0] = '\0';
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 262144u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int loaded = 0;
    int invalid_date_span = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0' && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[256] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            const char* p = line;
            uint32_t team_id = 0u;
            uint32_t league_id = 0u;
            uint32_t injured_player_id = 0u;
            uint32_t replacement_player_id = 0u;
            uint32_t opened_on = 0u;
            uint32_t expected_end = 0u;
            uint32_t slot_type = 0u;
            uint32_t status = 0u;
            uint32_t converted = 0u;
            if (line[0] >= '0' && line[0] <= '9'
                    && parse_u32_from_csv_field(&p, &team_id)
                    && parse_u32_from_csv_field(&p, &league_id)
                    && parse_u32_from_csv_field(&p, &injured_player_id)
                    && parse_u32_from_csv_field(&p, &replacement_player_id)
                    && parse_u32_from_csv_field(&p, &opened_on)
                    && parse_u32_from_csv_field(&p, &expected_end)
                    && parse_u32_from_csv_field(&p, &slot_type)
                    && parse_u32_from_csv_field(&p, &status)
                    && parse_u32_from_csv_field(&p, &converted)
                    && team_id != 0u
                    && injured_player_id != 0u
                    && slot_type >= KBO_FOREIGN_INJURY_SLOT_REGULAR
                    && slot_type <= KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                    && status >= KBO_FOREIGN_INJURY_STATUS_OPEN
                    && status <= KBO_FOREIGN_INJURY_STATUS_CLOSED) {
                if (!kbo_foreign_injury_persisted_date_span_plausible(opened_on, expected_end)) {
                    invalid_date_span++;
                    if (invalid_date_span <= 20) {
                        append_logf(
                            "foreign injury replacement: discarded persisted record with invalid date span team=%u injured=%u opened=%u expected_end=%u status=%u path=%s",
                            team_id,
                            injured_player_id,
                            opened_on,
                            expected_end,
                            status,
                            path);
                    }
                    goto next_line;
                }
                KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
                rec->team_id = team_id;
                rec->league_id = league_id;
                rec->injured_player_id = injured_player_id;
                rec->replacement_player_id = replacement_player_id;
                rec->opened_on_yyyymmdd = opened_on;
                rec->expected_end_yyyymmdd = expected_end;
                rec->slot_type = (uint8_t)slot_type;
                rec->status = (uint8_t)status;
                rec->converted = converted ? 1u : 0u;
                loaded++;
            }

next_line:
            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    append_logf(
        "foreign injury replacement: loaded=%d invalid_date_span=%d path=%s",
        loaded,
        invalid_date_span,
        path);
    return loaded;
}

int kbo_parse_foreign_injury_replacement_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    const char* p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    if (kbo_parse_foreign_injury_replacement_key_seed_line(line, today, out)) {
        goto resolve_context;
    }

    uint32_t values[9] = {0};
    int count = 0;
    while (count < 9) {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p < '0' || *p > '9') {
            break;
        }
        if (!parse_u32_from_csv_field(&p, &values[count])) {
            break;
        }
        count++;
        while (*p != '\0' && *p != ',' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p == '\0' || *p == '\r' || *p == '\n') {
            break;
        }
    }

    if (count < 3) {
        return 0;
    }

    if (count >= 9) {
        out->team_id = values[0];
        out->league_id = values[1];
        out->injured_player_id = values[2];
        out->replacement_player_id = values[3];
        out->opened_on_yyyymmdd = values[4];
        out->expected_end_yyyymmdd = values[5];
        out->slot_type = (uint8_t)values[6];
        out->status = (uint8_t)values[7];
        out->converted = values[8] ? 1u : 0u;
    } else {
        out->team_id = values[0];
        out->injured_player_id = values[1];
        out->replacement_player_id = values[2];
        out->slot_type = (count >= 4) ? (uint8_t)values[3] : 0u;
        out->status = (count >= 5) ? (uint8_t)values[4] : 0u;
        out->opened_on_yyyymmdd = today;
    }

    if (out->injured_player_id == 0u) {
        return 0;
    }

resolve_context:
    uint32_t injured_team_id = 0u;
    uint32_t injured_league_id = 0u;
    uint32_t replacement_team_id = 0u;
    uint32_t replacement_league_id = 0u;
    uint8_t* injured = kbo_find_player_by_id(out->injured_player_id, &injured_team_id, &injured_league_id);
    uint8_t* replacement = kbo_find_player_by_id(out->replacement_player_id, &replacement_team_id, &replacement_league_id);

    if (out->team_id == 0u) {
        out->team_id = injured_team_id != 0u ? injured_team_id : replacement_team_id;
    }
    if (out->league_id == 0u) {
        out->league_id = injured_league_id != 0u ? injured_league_id : replacement_league_id;
    }
    if (out->league_id == 0u && out->team_id != 0u) {
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(out->team_id, 1);
        if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            out->league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        }
    }

    if (out->opened_on_yyyymmdd == 0u) {
        out->opened_on_yyyymmdd = today;
    }
    if ((out->slot_type < KBO_FOREIGN_INJURY_SLOT_REGULAR || out->slot_type > KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA)) {
        if (injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            out->slot_type = kbo_foreign_injury_slot_type_for_player(injured);
        } else if (replacement != NULL && memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
            out->slot_type = kbo_foreign_injury_slot_type_for_player(replacement);
        } else {
            out->slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
    }
    if (out->status < KBO_FOREIGN_INJURY_STATUS_OPEN || out->status > KBO_FOREIGN_INJURY_STATUS_CLOSED) {
        out->status = out->replacement_player_id != 0u ? KBO_FOREIGN_INJURY_STATUS_ACTIVE : KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (out->replacement_player_id == 0u && out->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
        out->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (out->expected_end_yyyymmdd == 0u
            && injured != NULL
            && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        if (days_left > 0 && today != 0u) {
            out->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)days_left);
        }
    }

    return out->team_id != 0u;
}

int kbo_import_foreign_injury_replacement_seed_file_locked(
    const char* path,
    uint32_t today,
    const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 262144u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int imported = 0;
    int skipped = 0;
    int parse_failed = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[256] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            KboForeignInjuryReplacement rec;
            if (kbo_parse_foreign_injury_replacement_seed_line(line, today, &rec)) {
                if (kbo_find_foreign_injury_replacement_locked(rec.injured_player_id, 1) >= 0) {
                    skipped++;
                } else if (g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
                    g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++] = rec;
                    imported++;
                }
            } else {
                char* check = line;
                while (*check == ' ' || *check == '\t') {
                    check++;
                }
                if (*check != '\0' && *check != '#') {
                    parse_failed++;
                }
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (imported > 0 || skipped > 0 || parse_failed > 0) {
        append_logf(
            "foreign injury replacement: seed import source=%s imported=%d skipped=%d parse_failed=%d path=%s",
            source != NULL ? source : "",
            imported,
            skipped,
            parse_failed,
            path);
    }
    return imported;
}

