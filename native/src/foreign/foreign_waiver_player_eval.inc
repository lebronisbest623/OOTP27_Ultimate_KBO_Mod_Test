/* Foreign waiver player identity and value evaluation. Included from native/src/foreign_waiver_ai.inc. */

static int16_t kbo_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(int16_t))) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

static int32_t kbo_foreign_waiver_value_score(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    return talent * 55 + overall * 25 + ratings * 10 + career * 10;
}

static int32_t kbo_get_foreign_waiver_value_threshold(void)
{
    uint32_t configured = read_u32_leading_number_from_file("foreign_waiver_value_threshold.txt");
    if (configured != 0u && configured <= 20000u) {
        return (int32_t)configured;
    }
    return 3500;
}

static uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id)
{
    if (player_id == 0) {
        return NULL;
    }

    if (out_current_team_id != NULL) { *out_current_team_id = 0; }
    if (out_current_league_id != NULL) { *out_current_league_id = 0; }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t candidate_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (candidate_id != player_id) {
            continue;
        }

        if (out_current_team_id != NULL) {
            *out_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        }
        if (out_current_league_id != NULL) {
            *out_current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        }
        return player;
    }
    return NULL;
}

static int kbo_player_is_foreign_for_kbo_rights(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
}

#define KBO_ASIAN_QUOTA_NATION_MAX 64

static uint32_t g_kbo_asian_quota_nation_ids[KBO_ASIAN_QUOTA_NATION_MAX] = {0};
static LONG g_kbo_asian_quota_nation_count = -1;

static void kbo_add_asian_quota_nation_id(uint32_t nation_id, int* count)
{
    if (count == NULL || *count >= KBO_ASIAN_QUOTA_NATION_MAX
            || nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (g_kbo_asian_quota_nation_ids[i] == nation_id) {
            return;
        }
    }
    g_kbo_asian_quota_nation_ids[(*count)++] = nation_id;
}

static int kbo_load_asian_quota_nation_ids_once(void)
{
    LONG cached = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -1, -1);
    if (cached >= 0) {
        return (int)cached;
    }

    LONG marker = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -2, -1);
    if (marker != -1) {
        while ((cached = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -2, -2)) == -2) {
            Sleep(0);
        }
        return cached > 0 ? (int)cached : 0;
    }

    int count = 0;
    kbo_add_asian_quota_nation_id(98u, &count); /* Japan */
    kbo_add_asian_quota_nation_id(12u, &count); /* Australia */
    kbo_add_asian_quota_nation_id(43u, &count); /* Taiwan */

    char path[MAX_PATH] = {0};
    if (get_kbo_asian_quota_nation_ids_path(path, sizeof(path))) {
        HANDLE file = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            char raw[4096] = {0};
            DWORD read = 0;
            if (ReadFile(file, raw, sizeof(raw) - 1, &read, NULL) && read > 0) {
                raw[read < sizeof(raw) ? read : sizeof(raw) - 1] = '\0';
                char* cursor = raw;
                while (*cursor != '\0' && count < KBO_ASIAN_QUOTA_NATION_MAX) {
                    while (*cursor != '\0' && (*cursor < '0' || *cursor > '9')) {
                        cursor++;
                    }
                    if (*cursor == '\0') {
                        break;
                    }
                    char* tail = cursor;
                    unsigned long value = strtoul(cursor, &tail, 10);
                    if (tail == cursor) {
                        break;
                    }
                    if (value > 0ul && value <= 1000000ul) {
                        kbo_add_asian_quota_nation_id((uint32_t)value, &count);
                    }
                    cursor = tail;
                }
            }
            CloseHandle(file);
        }
    }

    append_logf("asian quota: loaded nation_ids=%d path=%s", count, path);
    InterlockedExchange(&g_kbo_asian_quota_nation_count, (LONG)count);
    return count;
}

static int kbo_nation_is_asian_quota_candidate(uint32_t nation_id)
{
    if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }

    int count = kbo_load_asian_quota_nation_ids_once();
    for (int i = 0; i < count; i++) {
        if (g_kbo_asian_quota_nation_ids[i] == nation_id) {
            return 1;
        }
    }
    return 0;
}

#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2

static int kbo_player_is_asian_quota_candidate(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return kbo_nation_is_asian_quota_candidate(*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET));
}

