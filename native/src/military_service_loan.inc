typedef void (__fastcall *OotpMilitaryServiceEntryFn)(void* player);

static int32_t kbo_military_days_left(uint8_t* player);
static int32_t kbo_military_effective_days_left(uint8_t* player);
static uint32_t kbo_military_effective_return_yyyymmdd(uint8_t* player);
static void    kbo_set_military_days_left(uint8_t* player, int32_t days_left);
static int     find_active_kbo_military_loan_index(uint32_t player_id);
static void    register_active_kbo_military_loan(
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t service_league_id);
static void    clear_kbo_military_unavailable_flags(uint8_t* player);
static uint8_t* kbo_military_find_player_by_id(uint32_t player_id);

/* ---- Date serial helpers ---- */

static uint32_t kbo_date_serial(uint32_t year, uint32_t month, uint32_t day)
{
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    uint32_t y = year - 1u;
    uint32_t serial = y * 365u + y / 4u - y / 100u + y / 400u;
    serial += days_before_month[month - 1u];
    if (month > 2 && kbo_is_leap_year(year)) {
        serial++;
    }
    serial += day;
    return serial;
}

static uint32_t kbo_current_date_serial(void)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    return kbo_date_serial(year, month, day);
}

/* ---- Seeded service assignments ---- */

#define KBO_MILITARY_SERVICE_SEED_MAX        512
#define KBO_MILITARY_SERVICE_SEED_KEY_BYTES  40
#define KBO_MILITARY_SERVICE_TEAM_BYTES      12

typedef struct KboMilitaryServiceSeed {
    char key[KBO_MILITARY_SERVICE_SEED_KEY_BYTES];
    uint32_t player_id;
    char service_team_code[KBO_MILITARY_SERVICE_TEAM_BYTES];
    char original_team_code[KBO_MILITARY_SERVICE_TEAM_BYTES];
    uint32_t service_start_yyyymmdd;
    uint32_t service_return_yyyymmdd;
    int32_t service_total_days;
} KboMilitaryServiceSeed;

typedef struct KboMilitaryDraftCandidate {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint16_t entry_year;
    uint8_t selected;
    uintptr_t player_ptr;
} KboMilitaryDraftCandidate;

typedef struct KboMilitarySelectionNewsEntry {
    uint32_t player_id;
    uint32_t original_team_id;
    int32_t score;
    uintptr_t player_ptr;
} KboMilitarySelectionNewsEntry;

typedef struct KboMilitaryReturnHistoryKey {
    uint32_t player_id;
    uint32_t history_yyyymmdd;
} KboMilitaryReturnHistoryKey;

static KboMilitaryServiceSeed g_kbo_military_service_seeds[KBO_MILITARY_SERVICE_SEED_MAX];
static int g_kbo_military_service_seed_count = 0;
static LONG g_kbo_military_service_seed_lock = 0;
static LONG g_kbo_military_service_seed_loaded = 0;
static ULONGLONG g_kbo_military_service_seed_last_resolve_tick = 0;
static char g_kbo_military_service_seed_loaded_key[MAX_PATH * 3];
static KboMilitaryDraftCandidate g_kbo_military_draft_candidates[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_kbo_military_draft_candidate_count = 0;
static KboMilitaryReturnHistoryKey g_kbo_military_return_history_keys[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_kbo_military_return_history_key_count = 0;

static void kbo_lock_military_service_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_military_service_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_military_service_seeds(void)
{
    InterlockedExchange(&g_kbo_military_service_seed_lock, 0);
}

static int kbo_find_military_draft_candidate_index(uint32_t player_id, uint16_t entry_year)
{
    if (player_id == 0u || entry_year == 0u) {
        return -1;
    }
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == player_id && candidate->entry_year == entry_year) {
            return (int)i;
        }
    }
    return -1;
}

static int kbo_queue_military_draft_candidate(
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t entry_year,
    uint32_t original_team_id,
    uint32_t original_league_id,
    const char* source)
{
    if (player_ptr == 0 || player_id == 0u || entry_year == 0u || original_team_id == 0u) {
        return 0;
    }

    int existing = kbo_find_military_draft_candidate_index(player_id, entry_year);
    if (existing >= 0) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[existing];
        candidate->player_ptr = player_ptr;
        if (original_team_id != 0u) {
            candidate->original_team_id = original_team_id;
        }
        if (original_league_id != 0u) {
            candidate->original_league_id = original_league_id;
        }
        append_logf(
            "KBO military draft candidate refreshed source=%s player_id=%u year=%u original_team=%u original_league=%u selected=%u player=%p",
            source != NULL ? source : "",
            player_id,
            entry_year,
            candidate->original_team_id,
            candidate->original_league_id,
            (uint32_t)candidate->selected,
            (void*)player_ptr);
        return 1;
    }

    LONG slot = InterlockedIncrement(&g_kbo_military_draft_candidate_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_kbo_military_draft_candidate_count);
        return 0;
    }

    KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[slot];
    memset(candidate, 0, sizeof(*candidate));
    candidate->player_id = player_id;
    candidate->player_ptr = player_ptr;
    candidate->entry_year = entry_year;
    candidate->original_team_id = original_team_id;
    candidate->original_league_id = original_league_id;
    append_logf(
        "KBO military draft candidate queued source=%s slot=%ld player_id=%u year=%u original_team=%u original_league=%u player=%p",
        source != NULL ? source : "",
        slot,
        player_id,
        entry_year,
        original_team_id,
        original_league_id,
        (void*)player_ptr);
    return 1;
}

static int kbo_count_military_draft_candidates_for_year(uint16_t entry_year)
{
    if (entry_year == 0u) {
        return 0;
    }
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    int queued = 0;
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id != 0u && candidate->entry_year == entry_year && candidate->selected == 0u) {
            queued++;
        }
    }
    return queued;
}

static int kbo_mark_military_return_history_once(uint32_t player_id, uint32_t history_yyyymmdd)
{
    if (player_id == 0u || history_yyyymmdd == 0u) {
        return 0;
    }
    LONG count = g_kbo_military_return_history_key_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryReturnHistoryKey* key = &g_kbo_military_return_history_keys[i];
        if (key->player_id == player_id && key->history_yyyymmdd == history_yyyymmdd) {
            return 0;
        }
    }

    LONG slot = InterlockedIncrement(&g_kbo_military_return_history_key_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_kbo_military_return_history_key_count);
        return 0;
    }
    g_kbo_military_return_history_keys[slot].player_id = player_id;
    g_kbo_military_return_history_keys[slot].history_yyyymmdd = history_yyyymmdd;
    return 1;
}

static void kbo_military_trim_csv_token_in_place(char* text)
{
    if (text == NULL) {
        return;
    }
    char* start = text;
    while (*start == ' ' || *start == '\t' || *start == '"') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start
            && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'
                || end[-1] == '\n' || end[-1] == '"')) {
        end--;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }
}

static int kbo_military_ascii_is_seed_id_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static int kbo_military_parse_u32_full_token(const char* text, uint32_t* out)
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

    unsigned long value = 0ul;
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10ul + (unsigned long)(*p - '0');
        if (value > 0xfffffffful) {
            return 0;
        }
    }

    *out = (uint32_t)value;
    return 1;
}

static uint32_t kbo_military_parse_yyyymmdd(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }

    char digits[9] = {0};
    size_t count = 0;
    for (const char* p = text; *p != '\0' && count < 8u; p++) {
        if (*p >= '0' && *p <= '9') {
            digits[count++] = *p;
        }
    }
    if (count != 8u) {
        return 0u;
    }
    uint32_t value = (uint32_t)strtoul(digits, NULL, 10);
    uint32_t month = (value / 100u) % 100u;
    uint32_t day = value % 100u;
    return (month >= 1u && month <= 12u && day >= 1u && day <= 31u) ? value : 0u;
}

static uint32_t kbo_military_yyyymmdd_to_serial(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return 0u;
    }
    return kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
}

static uint32_t kbo_military_days_in_month(uint32_t year, uint32_t month)
{
    static const uint8_t days_by_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1u || month > 12u) {
        return 0u;
    }
    if (month == 2u && kbo_is_leap_year(year)) {
        return 29u;
    }
    return days_by_month[month - 1u];
}

static uint32_t kbo_military_serial_to_yyyymmdd(uint32_t serial)
{
    if (serial == 0u) {
        return 0u;
    }

    uint32_t year = serial / 366u + 1u;
    while (kbo_date_serial(year + 1u, 1u, 1u) <= serial) {
        year++;
    }
    while (year > 1u && kbo_date_serial(year, 1u, 1u) > serial) {
        year--;
    }

    uint32_t month = 1u;
    while (month < 12u) {
        uint32_t next_month_serial = month == 12u
            ? kbo_date_serial(year + 1u, 1u, 1u)
            : kbo_date_serial(year, month + 1u, 1u);
        if (next_month_serial > serial) {
            break;
        }
        month++;
    }

    uint32_t month_start = kbo_date_serial(year, month, 1u);
    uint32_t day = serial >= month_start ? (serial - month_start + 1u) : 1u;
    uint32_t month_days = kbo_military_days_in_month(year, month);
    if (month_days != 0u && day > month_days) {
        day = month_days;
    }
    return year * 10000u + month * 100u + day;
}

static uint32_t kbo_military_yyyymmdd_add_days(uint32_t yyyymmdd, int32_t days)
{
    uint32_t serial = kbo_military_yyyymmdd_to_serial(yyyymmdd);
    if (serial == 0u || days < 0) {
        return 0u;
    }
    return kbo_military_serial_to_yyyymmdd(serial + (uint32_t)days);
}

static int32_t kbo_military_days_left_from_return_serial(uint32_t return_serial, uint32_t today_serial)
{
    if (return_serial == 0u || today_serial == 0u || return_serial <= today_serial) {
        return 0;
    }
    uint32_t diff = return_serial - today_serial;
    return diff > 32767u ? 32767 : (int32_t)diff;
}

static int kbo_get_save_military_service_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("military_service_seed.csv", out, out_size);
}

static int kbo_get_global_military_service_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    char* local_app_data = getenv("LOCALAPPDATA");
    if (local_app_data == NULL || local_app_data[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\military_service_seed.csv", local_app_data);
    return 1;
}

static int kbo_get_save_military_service_resolved_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("military_service_resolved.csv", out, out_size);
}

static int kbo_get_current_players_dat_path_for_military_seed(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\players.dat", save_path);
    DWORD attrs = GetFileAttributesA(out);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return 1;
    }

    snprintf(out, out_size, "%s\\data\\players.dat", save_path);
    attrs = GetFileAttributesA(out);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int kbo_parse_military_service_seed_line(const char* line, KboMilitaryServiceSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[240] = {0};
    size_t len = strlen(line);
    if (len >= sizeof(copy)) {
        len = sizeof(copy) - 1u;
    }
    memcpy(copy, line, len);

    char* p = copy;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    char* fields[8] = {0};
    int field_count = 0;
    fields[field_count++] = p;
    for (char* q = p; *q != '\0' && field_count < 8; q++) {
        if (*q == ',') {
            *q = '\0';
            fields[field_count++] = q + 1;
        }
    }
    for (int i = 0; i < field_count; i++) {
        kbo_military_trim_csv_token_in_place(fields[i]);
    }
    if (fields[0] == NULL || fields[0][0] == '\0'
            || _stricmp(fields[0], "source_key") == 0
            || _stricmp(fields[0], "player_id") == 0) {
        return 0;
    }

    snprintf(out->key, sizeof(out->key), "%s", fields[0]);
    uint32_t parsed_u32 = 0u;
    if (kbo_military_parse_u32_full_token(out->key, &parsed_u32)) {
        out->player_id = parsed_u32;
    }

    if (field_count == 3
            && fields[1][0] >= '0' && fields[1][0] <= '9'
            && fields[2][0] >= '0' && fields[2][0] <= '9') {
        uint32_t today_serial = kbo_current_date_serial();
        uint32_t days_left = (uint32_t)strtoul(fields[2], NULL, 10);
        snprintf(out->service_team_code, sizeof(out->service_team_code), "SANG");
        snprintf(out->original_team_code, sizeof(out->original_team_code), "%s", fields[1]);
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        out->service_return_yyyymmdd = today_serial != 0u
            ? kbo_military_serial_to_yyyymmdd(today_serial + days_left)
            : 0u;
        return out->key[0] != '\0' || out->player_id != 0u;
    }

    if (field_count > 5 && fields[5][0] != '\0'
            && kbo_military_parse_u32_full_token(fields[5], &parsed_u32)) {
        out->player_id = parsed_u32;
    }
    snprintf(out->service_team_code, sizeof(out->service_team_code), "%s",
        (field_count > 1 && fields[1][0] != '\0') ? fields[1] : "SANG");
    if (field_count > 2 && fields[2][0] != '\0') {
        snprintf(out->original_team_code, sizeof(out->original_team_code), "%s", fields[2]);
    }

    uint32_t numeric_field4 = 0u;
    int field4_is_numeric = field_count > 4
        && fields[4][0] != '\0'
        && kbo_military_parse_u32_full_token(fields[4], &numeric_field4);
    int looks_like_old_start_total = field_count > 5
        && field4_is_numeric
        && kbo_military_parse_yyyymmdd(fields[3]) != 0u;

    if (looks_like_old_start_total) {
        out->service_start_yyyymmdd = kbo_military_parse_yyyymmdd(fields[3]);
        out->service_total_days = (int32_t)numeric_field4;
        out->service_return_yyyymmdd = kbo_military_yyyymmdd_add_days(
            out->service_start_yyyymmdd,
            out->service_total_days > 0 ? out->service_total_days : KBO_MILITARY_SERVICE_DAYS);
    } else {
        out->service_return_yyyymmdd = field_count > 3 ? kbo_military_parse_yyyymmdd(fields[3]) : 0u;
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        if (field_count > 4 && field4_is_numeric) {
            out->player_id = numeric_field4;
        }
    }
    if (out->service_total_days <= 0) {
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
    }
    return out->key[0] != '\0' || out->player_id != 0u;
}

static int kbo_add_military_service_seed_locked(const KboMilitaryServiceSeed* seed)
{
    if (seed == NULL || (seed->key[0] == '\0' && seed->player_id == 0u)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* existing = &g_kbo_military_service_seeds[i];
        if ((seed->player_id != 0u && existing->player_id == seed->player_id)
                || (seed->key[0] != '\0' && existing->key[0] != '\0'
                    && _stricmp(existing->key, seed->key) == 0)) {
            if (existing->player_id == 0u) {
                existing->player_id = seed->player_id;
            }
            if (seed->service_team_code[0] != '\0') {
                snprintf(existing->service_team_code, sizeof(existing->service_team_code), "%s", seed->service_team_code);
            }
            if (seed->original_team_code[0] != '\0') {
                snprintf(existing->original_team_code, sizeof(existing->original_team_code), "%s", seed->original_team_code);
            }
            if (seed->service_start_yyyymmdd != 0u) {
                existing->service_start_yyyymmdd = seed->service_start_yyyymmdd;
            }
            if (seed->service_return_yyyymmdd != 0u) {
                existing->service_return_yyyymmdd = seed->service_return_yyyymmdd;
            }
            if (seed->service_total_days > 0) {
                existing->service_total_days = seed->service_total_days;
            }
            return 1;
        }
    }
    if (g_kbo_military_service_seed_count >= KBO_MILITARY_SERVICE_SEED_MAX) {
        return 0;
    }
    g_kbo_military_service_seeds[g_kbo_military_service_seed_count++] = *seed;
    return 1;
}

static int kbo_load_military_service_seed_file_locked(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    int loaded = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        KboMilitaryServiceSeed seed;
        if (kbo_parse_military_service_seed_line(line, &seed)
                && kbo_add_military_service_seed_locked(&seed)) {
            loaded++;
        }
    }
    fclose(fp);
    if (loaded > 0) {
        append_logf("KBO military service seed loaded=%d path=%s", loaded, path);
    }
    return loaded;
}

static int kbo_load_military_service_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_military_service_resolved_path(path, sizeof(path))) {
        return 0;
    }
    return kbo_load_military_service_seed_file_locked(path);
}

static int kbo_persist_military_service_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_military_service_resolved_path(path, sizeof(path))) {
        return 0;
    }
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        append_logf("KBO military service resolved cache persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }
    fprintf(fp, "source_key,service_team,original_team,service_return_yyyymmdd,player_id\n");
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id == 0u || seed->key[0] == '\0') {
            continue;
        }
        uint32_t return_yyyymmdd = seed->service_return_yyyymmdd;
        if (return_yyyymmdd == 0u && seed->service_start_yyyymmdd != 0u) {
            return_yyyymmdd = kbo_military_yyyymmdd_add_days(
                seed->service_start_yyyymmdd,
                seed->service_total_days > 0 ? seed->service_total_days : KBO_MILITARY_SERVICE_DAYS);
        }
        fprintf(fp, "%s,%s,%s,%u,%u\n",
            seed->key,
            seed->service_team_code[0] != '\0' ? seed->service_team_code : "SANG",
            seed->original_team_code,
            return_yyyymmdd,
            seed->player_id);
    }
    fclose(fp);
    return 1;
}

static int kbo_military_player_memory_contains_seed_key(uint8_t* player, const char* key)
{
    if (player == NULL || key == NULL || key[0] == '\0' || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    size_t key_len = strlen(key);
    if (key_len < 3u || key_len >= KBO_MILITARY_SERVICE_SEED_KEY_BYTES || key_len >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }
    for (size_t i = 0; i + key_len < OOTP27_PLAYER_SCAN_BYTES; i++) {
        if (memcmp(player + i, key, key_len) != 0) {
            continue;
        }
        char before = i > 0u ? (char)player[i - 1u] : '\0';
        char after = (char)player[i + key_len];
        if (!kbo_military_ascii_is_seed_id_char(before) && !kbo_military_ascii_is_seed_id_char(after)) {
            return 1;
        }
    }
    return 0;
}

static int kbo_military_buffer_contains_u32_le(const uint8_t* data, size_t size, uint32_t value)
{
    if (data == NULL || size < 4u || value == 0u) {
        return 0;
    }
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    bytes[2] = (uint8_t)((value >> 16) & 0xffu);
    bytes[3] = (uint8_t)((value >> 24) & 0xffu);
    for (size_t i = 0; i + 4u <= size; i++) {
        if (memcmp(data + i, bytes, sizeof(bytes)) == 0) {
            return 1;
        }
    }
    return 0;
}

static uint8_t* kbo_military_find_player_by_id(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
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
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static uint32_t kbo_military_resolve_player_id_from_players_dat_record_start(
    const uint8_t* raw,
    size_t read,
    size_t key_pos)
{
    if (raw == NULL || read < 16u || key_pos < 16u) {
        return 0u;
    }

    size_t min_start = key_pos > 512u ? key_pos - 512u : 0u;
    for (size_t start = key_pos - 16u; ; start--) {
        if (start + 16u <= read) {
            uint32_t candidate_id =
                (uint32_t)raw[start]
                | ((uint32_t)raw[start + 1u] << 8)
                | ((uint32_t)raw[start + 2u] << 16)
                | ((uint32_t)raw[start + 3u] << 24);
            uint32_t first_name_id =
                (uint32_t)raw[start + 4u]
                | ((uint32_t)raw[start + 5u] << 8)
                | ((uint32_t)raw[start + 6u] << 16)
                | ((uint32_t)raw[start + 7u] << 24);
            uint32_t last_name_id =
                (uint32_t)raw[start + 8u]
                | ((uint32_t)raw[start + 9u] << 8)
                | ((uint32_t)raw[start + 10u] << 16)
                | ((uint32_t)raw[start + 11u] << 24);
            uint8_t day = raw[start + 12u];
            uint8_t month = raw[start + 13u];
            uint16_t year = (uint16_t)raw[start + 14u] | ((uint16_t)raw[start + 15u] << 8);
            if (candidate_id != 0u
                    && first_name_id != 0u
                    && last_name_id != 0u
                    && day >= 1u && day <= 31u
                    && month >= 1u && month <= 12u
                    && year >= 1800u && year <= 2100u
                    && kbo_military_find_player_by_id(candidate_id) != NULL) {
                return candidate_id;
            }
        }
        if (start == min_start || start == 0u) {
            break;
        }
    }
    return 0u;
}

static uint32_t kbo_resolve_military_service_seed_key_from_memory(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0u;
    }
    if (key[0] >= '0' && key[0] <= '9') {
        return (uint32_t)strtoul(key, NULL, 10);
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0u;
    }

    uint32_t first_match = 0u;
    int matches = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_military_player_memory_contains_seed_key(player, key)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        if (first_match == 0u) {
            first_match = player_id;
        }
        matches++;
    }
    if (first_match != 0u) {
        append_logf("KBO military service seed resolved key=%s player=%u matches=%d", key, first_match, matches);
    }
    return first_match;
}

static uint32_t kbo_resolve_military_service_seed_key_from_players_dat(const char* key)
{
    if (key == NULL || key[0] == '\0' || (key[0] >= '0' && key[0] <= '9')) {
        return 0u;
    }

    char players_dat_path[MAX_PATH] = {0};
    if (!kbo_get_current_players_dat_path_for_military_seed(players_dat_path, sizeof(players_dat_path))) {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat missing or save not written", key);
        return 0u;
    }

    HANDLE file = CreateFileA(players_dat_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat open failed gle=%lu", key, (unsigned long)GetLastError());
        return 0u;
    }

    LARGE_INTEGER file_size;
    file_size.QuadPart = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(file);
        append_logf("KBO military service seed unresolved key=%s reason=players.dat size unsupported", key);
        return 0u;
    }

    uint8_t* raw = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)file_size.QuadPart);
    if (raw == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0;
    int read_ok = ReadFile(file, raw, (DWORD)file_size.QuadPart, &read, NULL) && read == (DWORD)file_size.QuadPart;
    CloseHandle(file);
    if (!read_ok) {
        HeapFree(GetProcessHeap(), 0, raw);
        append_logf("KBO military service seed unresolved key=%s reason=players.dat read failed", key);
        return 0u;
    }

    size_t key_len = strlen(key);
    uint32_t matched_player_id = 0u;
    int ambiguous = 0;
    for (size_t pos = 0; pos + key_len <= (size_t)read; pos++) {
        if (memcmp(raw + pos, key, key_len) != 0) {
            continue;
        }
        char before = pos > 0u ? (char)raw[pos - 1u] : '\0';
        char after = (char)raw[pos + key_len];
        if (kbo_military_ascii_is_seed_id_char(before) || kbo_military_ascii_is_seed_id_char(after)) {
            continue;
        }

        uint32_t record_player_id = kbo_military_resolve_player_id_from_players_dat_record_start(raw, (size_t)read, pos);
        if (record_player_id != 0u) {
            if (matched_player_id != 0u && matched_player_id != record_player_id) {
                ambiguous = 1;
                break;
            }
            matched_player_id = record_player_id;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (ambiguous) {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat ambiguous", key);
        return 0u;
    }
    if (matched_player_id != 0u) {
        append_logf("KBO military service seed resolved via players.dat key=%s player=%u", key, matched_player_id);
    } else {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat no matching player record header", key);
    }
    return matched_player_id;
}

static int kbo_resolve_military_service_seeds_locked(void)
{
    int resolved = 0;
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id != 0u || seed->key[0] == '\0') {
            continue;
        }
        uint32_t player_id = 0u;
        if (seed->key[0] >= '0' && seed->key[0] <= '9') {
            player_id = (uint32_t)strtoul(seed->key, NULL, 10);
        } else {
            player_id = kbo_resolve_military_service_seed_key_from_players_dat(seed->key);
        }
        if (player_id != 0u) {
            seed->player_id = player_id;
            resolved++;
        }
    }
    if (resolved > 0) {
        kbo_persist_military_service_resolved_cache_locked();
    }
    return resolved;
}

static void kbo_ensure_military_service_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    char resolved_path[MAX_PATH] = {0};
    char loaded_key[MAX_PATH * 3] = {0};
    kbo_get_save_military_service_seed_path(save_seed_path, sizeof(save_seed_path));
    kbo_get_global_military_service_seed_path(global_seed_path, sizeof(global_seed_path));
    kbo_get_save_military_service_resolved_path(resolved_path, sizeof(resolved_path));
    snprintf(
        loaded_key,
        sizeof(loaded_key),
        "%s|%s|%s",
        save_seed_path,
        global_seed_path,
        resolved_path);

    int should_reload = 0;
    if (InterlockedCompareExchange(&g_kbo_military_service_seed_loaded, 1, 0) == 0) {
        should_reload = 1;
    } else if (_stricmp(g_kbo_military_service_seed_loaded_key, loaded_key) != 0) {
        should_reload = 1;
    }

    if (should_reload) {
        kbo_lock_military_service_seeds();
        g_kbo_military_service_seed_count = 0;
        snprintf(g_kbo_military_service_seed_loaded_key, sizeof(g_kbo_military_service_seed_loaded_key), "%s", loaded_key);
        g_kbo_military_service_seed_last_resolve_tick = GetTickCount64();
        kbo_load_military_service_seed_file_locked(save_seed_path);
        if (global_seed_path[0] != '\0'
                && (save_seed_path[0] == '\0' || _stricmp(save_seed_path, global_seed_path) != 0)) {
            kbo_load_military_service_seed_file_locked(global_seed_path);
        }
        kbo_load_military_service_resolved_cache_locked();
        kbo_resolve_military_service_seeds_locked();
        kbo_unlock_military_service_seeds();
        return;
    }

    ULONGLONG now = GetTickCount64();
    ULONGLONG last_tick = g_kbo_military_service_seed_last_resolve_tick;
    if (last_tick == 0ull || now < last_tick || now - last_tick > 5000ull) {
        g_kbo_military_service_seed_last_resolve_tick = now;
        kbo_lock_military_service_seeds();
        kbo_resolve_military_service_seeds_locked();
        kbo_unlock_military_service_seeds();
    }
}

static int kbo_apply_military_service_seed_assignments(uint8_t* sang, uint8_t* kpb, const char* source)
{
    if (sang == NULL && kpb == NULL) {
        return 0;
    }
    kbo_ensure_military_service_seeds_loaded();

    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t today_serial = kbo_current_date_serial();
    int applied = 0;

    kbo_lock_military_service_seeds();
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_military_find_player_by_id(seed->player_id);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            continue;
        }

        uint8_t* service_team = (seed->service_team_code[0] != '\0' && _stricmp(seed->service_team_code, "KPB") == 0)
            ? kpb
            : sang;
        uint32_t service_team_id = service_team != NULL
            ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_ID_OFFSET)
            : 0u;
        if (service_team_id == 0u) {
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        if (seed->original_team_code[0] != '\0') {
            uint8_t* original_team = NULL;
            if (seed->original_team_code[0] >= '0' && seed->original_team_code[0] <= '9') {
                original_team = find_kbo_team_by_numeric_id_any_league((uint32_t)strtoul(seed->original_team_code, NULL, 10), 0);
            } else {
                original_team = find_kbo_team_by_csv_id_any_league(seed->original_team_code, 0);
            }
            if (original_team != NULL) {
                original_team_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_ID_OFFSET);
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }
        if (original_team_id == 0u) {
            original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            original_league_id = *(uint32_t*)(player + 0x4c);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            original_team_id = *(uint32_t*)(player + 0x5c);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            continue;
        }
        if (original_league_id == 0u) {
            uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
            if (original_team != NULL) {
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            int called_pre_change = 0;
            int called_register = 0;
            int called_attach = 0;
            uint32_t service_league_id = *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_internal(
                player,
                service_team,
                service_league_id,
                0,
                &called_pre_change,
                &called_register,
                &called_attach);
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
            *(uint32_t*)(player + 0x5c) = original_team_id;
            if (original_league_id != 0u) {
                *(uint32_t*)(player + 0x4c) = original_league_id;
            }
            append_logf(
                "KBO military service seed assigned to service key=%s player=%u old_team=%u service_team=%u original_team=%u pre=%d register=%d attach=%d",
                seed->key,
                seed->player_id,
                current_team_id,
                service_team_id,
                original_team_id,
                called_pre_change,
                called_register,
                called_attach);
        }

        register_active_kbo_military_loan(
            seed->player_id,
            (uintptr_t)player,
            original_team_id,
            original_league_id,
            service_team_id,
            *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET));

        int active_index = find_active_kbo_military_loan_index(seed->player_id);
        if (active_index >= 0) {
            KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
            uint32_t start_serial = kbo_military_yyyymmdd_to_serial(seed->service_start_yyyymmdd);
            if (start_serial != 0u) {
                loan->service_start_date_serial = start_serial;
            }
            loan->service_total_days = seed->service_total_days > 0
                ? seed->service_total_days
                : KBO_MILITARY_SERVICE_DAYS;
            uint32_t return_serial = kbo_military_yyyymmdd_to_serial(seed->service_return_yyyymmdd);
            if (return_serial == 0u && loan->service_start_date_serial != 0u) {
                return_serial = loan->service_start_date_serial + (uint32_t)loan->service_total_days;
            }
            if (return_serial != 0u) {
                loan->service_return_date_serial = return_serial;
            }
            if (today_serial != 0u && loan->service_return_date_serial != 0u) {
                kbo_set_military_days_left(
                    player,
                    kbo_military_days_left_from_return_serial(loan->service_return_date_serial, today_serial));
            }
            player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1;
            clear_kbo_military_unavailable_flags(player);
            applied++;
        }
    }
    kbo_unlock_military_service_seeds();

    if (applied > 0) {
        append_logf("KBO military service seed applied source=%s assignments=%d", source != NULL ? source : "", applied);
    }
    return applied;
}

/* ---- Active loan registry ---- */

static int find_active_kbo_military_loan_index(uint32_t player_id)
{
    if (player_id == 0) {
        return -1;
    }

    LONG count = g_active_military_loan_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    for (LONG i = 0; i < count; i++) {
        if (g_active_military_loans[i].player_id == player_id) {
            return (int)i;
        }
    }
    return -1;
}

static void register_active_kbo_military_loan(
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t service_league_id)
{
    if (player_id == 0 || service_team_id == 0) {
        return;
    }

    int existing = find_active_kbo_military_loan_index(player_id);
    if (existing >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[existing];
        loan->player_ptr = player_ptr;
        if (original_team_id  != 0) { loan->original_team_id  = original_team_id;  }
        if (original_league_id != 0) { loan->original_league_id = original_league_id; }
        loan->service_team_id   = service_team_id;
        loan->service_league_id = service_league_id;
        uint32_t today = kbo_current_date_serial();
        int32_t current_days_left = (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr))
            ? kbo_military_days_left((uint8_t*)player_ptr)
            : KBO_MILITARY_SERVICE_DAYS;
        if (loan->service_start_date_serial == 0) {
            if (current_days_left > 0
                    && current_days_left < KBO_MILITARY_SERVICE_DAYS
                    && today > (uint32_t)(KBO_MILITARY_SERVICE_DAYS - current_days_left)) {
                loan->service_start_date_serial = today - (uint32_t)(KBO_MILITARY_SERVICE_DAYS - current_days_left);
            } else {
                loan->service_start_date_serial = today;
            }
        }
        if (loan->service_total_days <= 0) {
            loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        }
        if (loan->service_return_date_serial == 0 && today != 0u) {
            if (current_days_left > 0) {
                loan->service_return_date_serial = today + (uint32_t)current_days_left;
            } else if (loan->service_start_date_serial != 0u) {
                loan->service_return_date_serial = loan->service_start_date_serial + (uint32_t)loan->service_total_days;
            }
        }
        if (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr)
                && current_days_left > KBO_MILITARY_SERVICE_DAYS) {
            kbo_set_military_days_left((uint8_t*)player_ptr, KBO_MILITARY_SERVICE_DAYS);
        }
        return;
    }

    LONG slot = InterlockedIncrement(&g_active_military_loan_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_active_military_loan_count);
        return;
    }

    KboMilitaryActiveLoan* loan = &g_active_military_loans[slot];
    loan->player_id              = player_id;
    loan->player_ptr             = player_ptr;
    loan->original_team_id       = original_team_id;
    loan->original_league_id     = original_league_id;
    loan->service_team_id        = service_team_id;
    loan->service_league_id      = service_league_id;
    loan->service_start_date_serial = kbo_current_date_serial();
    loan->service_total_days     = KBO_MILITARY_SERVICE_DAYS;
    int32_t current_days_left = (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr))
        ? kbo_military_days_left((uint8_t*)player_ptr)
        : KBO_MILITARY_SERVICE_DAYS;
    loan->service_return_date_serial = loan->service_start_date_serial != 0u
        ? loan->service_start_date_serial + (uint32_t)(current_days_left > 0 ? current_days_left : KBO_MILITARY_SERVICE_DAYS)
        : 0u;
    if (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr)) {
        kbo_set_military_days_left(
            (uint8_t*)player_ptr,
            current_days_left > 0 ? current_days_left : KBO_MILITARY_SERVICE_DAYS);
    }
}

static void unregister_active_kbo_military_loan(uint32_t player_id)
{
    int index = find_active_kbo_military_loan_index(player_id);
    if (index >= 0) {
        g_active_military_loans[index].player_id = 0;
    }
}

static int kbo_player_is_registered_active_military_loan(uintptr_t player_ptr)
{
    if (player_ptr == 0 || !memory_range_readable(
            (void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    return find_active_kbo_military_loan_index(player_id) >= 0;
}

/* ---- Player military state helpers ---- */

static int32_t kbo_military_days_left(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET, sizeof(int16_t))) {
        return 0;
    }
    return (int32_t)(*(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET));
}

static int32_t kbo_military_effective_days_left(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        uint32_t today_serial = kbo_current_date_serial();
        if (loan->service_return_date_serial != 0u && today_serial != 0u) {
            return kbo_military_days_left_from_return_serial(
                loan->service_return_date_serial,
                today_serial);
        }
    }
    return kbo_military_days_left(player);
}

static uint32_t kbo_military_effective_return_yyyymmdd(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0u;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        if (loan->service_return_date_serial != 0u) {
            return kbo_military_serial_to_yyyymmdd(loan->service_return_date_serial);
        }
    }

    uint32_t today_serial = kbo_current_date_serial();
    int32_t days_left = kbo_military_days_left(player);
    if (today_serial == 0u || days_left <= 0) {
        return 0u;
    }
    return kbo_military_serial_to_yyyymmdd(today_serial + (uint32_t)days_left);
}

static void kbo_military_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_military_copy_team_history_name(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (team != NULL) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        if (full_name[0] != '\0') {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0') {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (nickname[0] != '\0') {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
    }
    snprintf(out, out_size, "%s", fallback != NULL ? fallback : "his original club");
}

static void kbo_military_copy_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (team_id == 0u) {
        return;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    char team_name[96] = {0};
    kbo_military_copy_team_history_name(team, team_name, sizeof(team_name), NULL);
    if (team_name[0] == '\0' || ascii_equals_ignore_case(team_name, "his original club")) {
        snprintf(team_name, sizeof(team_name), "Team #%u", team_id);
    }
    snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
}

static void kbo_military_copy_player_link(uint32_t player_id, uintptr_t player_ptr, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (player_id == 0u) {
        return;
    }

    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_military_find_player_by_id(player_id);
    }

    char player_name[64] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", player_id);
    }

    snprintf(out, out_size, "<%s:player#%u>", player_name, player_id);
}

static int kbo_emit_military_selection_news(
    uint32_t event_yyyymmdd,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source)
{
    if (event_yyyymmdd == 0u || entries == NULL || entry_count <= 0) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        append_logf(
            "KBO military selection news skipped source=%s date=%u reason=league_id_unavailable count=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            entry_count);
        return 0;
    }

    char title[160] = {0};
    snprintf(
        title,
        sizeof(title),
        "Sangmu Selects %d Player%s for Military Service",
        entry_count,
        entry_count == 1 ? "" : "s");

    char body[8192] = {0};
    int len = snprintf(
        body,
        sizeof(body),
        "Sangmu completed its annual military-service selection, adding %d player%s from the offseason military-service pool. The group will report to the service club while their original KBO clubs retain their rights until the scheduled return date.\n\nSelected players:\n",
        entry_count,
        entry_count == 1 ? "" : "s");
    if (len < 0) {
        body[0] = '\0';
    }
    size_t used = strlen(body);

    for (int i = 0; i < entry_count && used + 180u < sizeof(body); i++) {
        char player_link[96] = {0};
        char team_link[128] = {0};
        kbo_military_copy_player_link(entries[i].player_id, entries[i].player_ptr, player_link, sizeof(player_link));
        kbo_military_copy_team_link(entries[i].original_team_id, team_link, sizeof(team_link));
        len = snprintf(
            body + used,
            sizeof(body) - used,
            "%d. %s%s%s\n",
            i + 1,
            player_link[0] != '\0' ? player_link : "Selected player",
            team_link[0] != '\0' ? ", " : "",
            team_link);
        if (len <= 0) {
            break;
        }
        used += (size_t)len;
    }

    if (used + 220u < sizeof(body)) {
        snprintf(
            body + used,
            sizeof(body) - used,
            "\nThe selections were made from players newly marked for military service after the season, with player value and service-club roster space considered before the final list was posted.");
    }

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO military selection news source=%s date=%u league_id=%u count=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        league_id,
        entry_count,
        created);
    return created;
}

static void kbo_set_military_days_left(uint8_t* player, int32_t days_left)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET, sizeof(int16_t))) {
        return;
    }
    *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET) =
        days_left > 0 ? (int16_t)days_left : 0;
}

static void clear_kbo_military_unavailable_flags(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET]           = 0;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0;
    player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET]             = 0;
}

static void clear_kbo_military_status_flags(uint8_t* player)
{
    clear_kbo_military_unavailable_flags(player);
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 0;
    }
}

static void complete_kbo_military_service_status(uint8_t* player)
{
    clear_kbo_military_status_flags(player);
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = 1;
        kbo_set_military_days_left(player, 0);
    }
}

/* ---- Native loan helpers ---- */

static int kbo_player_native_on_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) > 0
        && player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0
        && player[OOTP27_PLAYER_DFA_FLAG_OFFSET] == 0
        && player[OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET] != 0;
}

static int kbo_clear_native_player_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    return !kbo_player_native_on_loan(player);
}

/* ---- Return player from service ---- */

static int return_completed_kbo_military_loan_player(
    uint8_t* player,
    const char* source,
    uint32_t vector_offset,
    int require_registered)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (require_registered && active_index < 0) {
        return 0;
    }

    uint32_t sang_id = 0;
    uint32_t kpb_id  = 0;
    uint32_t registered_service_team_id  = 0;
    uint32_t registered_original_team_id = 0;
    uint32_t registered_original_league_id = 0;

    if (active_index >= 0) {
        KboMilitaryActiveLoan* active = &g_active_military_loans[active_index];
        active->player_ptr              = (uintptr_t)player;
        registered_service_team_id      = active->service_team_id;
        registered_original_team_id     = active->original_team_id;
        registered_original_league_id   = active->original_league_id;
    } else {
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
        sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
        kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t service_team_id = 0;
    if (registered_service_team_id != 0) {
        service_team_id = registered_service_team_id;
    } else if (current_team_id != 0
               && (current_team_id == sang_id || current_team_id == kpb_id)) {
        service_team_id = current_team_id;
    } else {
        return 0;
    }

    uint32_t original_team_id =
        registered_original_team_id != 0
            ? registered_original_team_id
            : *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_league_id =
        registered_original_league_id != 0
            ? registered_original_league_id
            : *(uint32_t*)(player + 0x4c);

    if (original_team_id == 0
            || original_team_id == service_team_id
            || original_team_id == sang_id
            || original_team_id == kpb_id) {
        original_team_id = *(uint32_t*)(player + 0x5c);
    }
    if (original_team_id == 0
            || original_team_id == service_team_id
            || original_team_id == sang_id
            || original_team_id == kpb_id) {
        return 0;
    }

    int32_t days_left = kbo_military_effective_days_left(player);
    if (days_left > 0) {
        clear_kbo_military_unavailable_flags(player);
        return 0;
    }

    uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
    if (original_team == NULL) {
        return 0;
    }

    int called_pre_change = 0;
    int called_register   = 0;
    int called_attach     = 0;
    int native_loan_before  = kbo_player_native_on_loan(player);
    int native_loan_cleared = native_loan_before ? kbo_clear_native_player_loan(player) : 0;
    uint32_t fallback_league_id = original_league_id != 0
        ? original_league_id
        : *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);

    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != original_team_id) {
        kbo_assign_player_to_team_like_ootp(
            player, original_team, fallback_league_id,
            &called_pre_change, &called_register, &called_attach);
    }

    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET)   = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET]   = 0;
    player[OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET] = 1;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET]    = 0;
    complete_kbo_military_service_status(player);
    unregister_active_kbo_military_loan(player_id);

    uint8_t old_restricted     = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t old_secondary      = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t old_injury_active  = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    uint8_t old_mil_exempt     = player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET];
    uint8_t old_mil_active     = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];

    char history_date[16] = {0};
    uint32_t year = 0;
    uint32_t month = 1;
    uint32_t day = 1;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        if (!kbo_current_year_relaxed(&year) || year == 0) { year = 2001; }
    }
    kbo_current_history_date(history_date, sizeof(history_date), year, "military_service_return");
    char service_team_name[96] = {0};
    char original_team_name[96] = {0};
    char history_text[256] = {0};
    uint8_t* service_team = find_kbo_team_by_numeric_id_any_league(service_team_id, 0);
    kbo_military_copy_team_history_name(
        service_team,
        service_team_name,
        sizeof(service_team_name),
        service_team_id == sang_id ? "Sangmu Baseball Team" : "Korean Police Baseball Team");
    kbo_military_copy_team_history_name(
        original_team,
        original_team_name,
        sizeof(original_team_name),
        "his original KBO organization");
    snprintf(
        history_text,
        sizeof(history_text),
        "Completed military service with %s and returned to %s.",
        service_team_name,
        original_team_name);
    uint32_t history_yyyymmdd = year * 10000u + month * 100u + day;
    int history_inserted = 0;
    int history_skipped_duplicate = 0;
    if (kbo_mark_military_return_history_once(player_id, history_yyyymmdd)) {
        append_special_player_history_csv(
            player_id, (uint16_t)year, history_date,
            "military_service_return",
            history_text);
        history_inserted = insert_kbo_player_history_sql(
            player_id,
            year,
            month,
            day,
            history_text,
            "military_service_return");
    } else {
        history_skipped_duplicate = 1;
    }

    LONG log_index = InterlockedIncrement(&g_military_loan_return_log_count);
    if (log_index <= 120) {
        append_logf(
            "KBO military assignment returned #%ld source=%s player_id=%u"
            " service_team=%u original_team=%u"
            " current_team=%u current_league=%u org_team=%u days_left=%d"
            " native_loan_before=%d native_loan_cleared=%d history_inserted=%d history_duplicate=%d"
            " old_restricted=%u old_secondary=%u old_injury=%u"
            " old_mil_exempt=%u mil_exempt=%u old_mil_active=%u mil_active=%u"
            " pre_change=%d register=%d attach=%d player=%p vector_off=0x%x",
            log_index,
            source != NULL ? source : "",
            player_id,
            service_team_id,
            original_team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            days_left,
            native_loan_before,
            native_loan_cleared,
            history_inserted,
            history_skipped_duplicate,
            (uint32_t)old_restricted,
            (uint32_t)old_secondary,
            (uint32_t)old_injury_active,
            (uint32_t)old_mil_exempt,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET],
            (uint32_t)old_mil_active,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET],
            called_pre_change,
            called_register,
            called_attach,
            player,
            vector_offset);
    }

    return 1;
}

/* ---- Background tick ---- */

static int tick_kbo_military_service_days(const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }

    flush_pending_special_player_history_sql(source);

    uint32_t today_serial = kbo_current_date_serial();
    if (today_serial == 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t   player_count  = 0;
    uint32_t  vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        return 0;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    if (sang_id == 0 && kpb_id == 0) {
        return 0;
    }
    kbo_apply_military_service_seed_assignments(sang, kpb, source);

    int tracked          = 0;
    int monitored        = 0;
    int managed          = 0;
    int returned         = 0;
    int newly_registered = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (current_team_id == 0
                || (current_team_id != sang_id && current_team_id != kpb_id)) {
            continue;
        }

        tracked++;
        uint32_t player_id        = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t original_league_id = *(uint32_t*)(player + 0x4c);
        if (original_team_id == 0
                || original_team_id == current_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            original_team_id = *(uint32_t*)(player + 0x5c);
        }

        int active_index = find_active_kbo_military_loan_index(player_id);
        if (active_index < 0
                && original_team_id != 0
                && original_team_id != current_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            uint8_t* service_team   = (current_team_id == sang_id) ? sang : kpb;
            uint32_t service_league = service_team != NULL
                ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
                : 0;
            register_active_kbo_military_loan(
                player_id, player_ptr,
                original_team_id, original_league_id,
                current_team_id,
                service_league != 0 ? service_league
                    : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET));
            active_index = find_active_kbo_military_loan_index(player_id);
            newly_registered++;
        }

        if (active_index < 0) {
            int32_t days_left = kbo_military_days_left(player);
            if (days_left <= 0) {
                returned += return_completed_kbo_military_loan_player(player, source, vector_offset, 0);
                continue;
            }
            clear_kbo_military_unavailable_flags(player);
            continue;
        }

        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        loan->player_ptr = player_ptr;
        if (loan->service_total_days <= 0) {
            loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        }
        if (loan->service_start_date_serial == 0
                || loan->service_start_date_serial > today_serial) {
            loan->service_start_date_serial = today_serial;
        }
        if (loan->service_return_date_serial == 0u) {
            int32_t stored_days_left = kbo_military_days_left(player);
            loan->service_return_date_serial = today_serial + (uint32_t)(
                stored_days_left > 0 ? stored_days_left : loan->service_total_days);
        }

        int32_t days_left = kbo_military_effective_days_left(player);
        int32_t stored_days_left = kbo_military_days_left(player);
        int32_t managed_days_left = kbo_military_days_left_from_return_serial(
            loan->service_return_date_serial,
            today_serial);
        if (managed_days_left != days_left) {
            days_left = managed_days_left;
        }
        if (stored_days_left != managed_days_left) {
            kbo_set_military_days_left(player, managed_days_left);
            managed++;
        }
        if (days_left <= 0) {
            returned += return_completed_kbo_military_loan_player(player, source, vector_offset, 0);
            continue;
        }

        clear_kbo_military_unavailable_flags(player);
        monitored++;
    }

    LONG log_index = InterlockedIncrement(&g_military_days_tick_log_count);
    if (returned > 0 || newly_registered > 0 || managed > 0 || log_index <= 20) {
        append_logf(
            "KBO military service day tick source=%s date_serial=%u"
            " tracked=%d newly_registered=%d monitored=%d managed=%d returned=%d"
            " count=%d vector_off=0x%x",
            source != NULL ? source : "",
            today_serial,
            tracked, newly_registered, monitored, managed, returned,
            player_count, vector_offset);
    }

    return returned;
}

static DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO military service day tick thread started");
    uint32_t last_tick_date_serial = 0u;
    for (;;) {
        Sleep(10000);
        uint32_t current_date_serial = kbo_current_date_serial();
        if (current_date_serial != 0u && current_date_serial == last_tick_date_serial) {
            continue;
        }
        tick_kbo_military_service_days("military_days_tick");
        if (current_date_serial != 0u) {
            last_tick_date_serial = current_date_serial;
        }
    }
    return 0;
}

static void start_kbo_military_days_tick_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (!read_kbo_localappdata_flag_file("enable_military_days_tick.txt")) {
        append_log_line("KBO military service day tick thread disabled: enable_military_days_tick.txt is off");
        return;
    }
    if (InterlockedCompareExchange(&g_military_days_tick_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_military_days_tick_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_military_days_tick_started, 0);
        append_log_line("KBO military service day tick thread failed to start");
    }
}

static int kbo_count_players_on_team_with_military_days(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }
    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == team_id
                && kbo_military_effective_days_left(player) > 0) {
            count++;
        }
    }
    return count;
}

static int16_t kbo_military_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(int16_t))) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

static int kbo_military_draft_candidate_score(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return INT32_MIN;
    }
    int32_t overall = kbo_military_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_military_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_military_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_military_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int score = talent * 55 + overall * 25 + ratings * 10 + career * 10;

    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (age >= 21 && age <= 26) {
        score += 300 - (int)(age - 21u) * 20;
    } else if (age < 21) {
        score += 120 - (int)(21u - age) * 10;
    } else {
        score += 120 - (int)(age - 26u) * 15;
    }
    score += (int)(*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) % 97u);
    return score;
}

static int kbo_route_queued_military_draft_candidates(
    uint16_t entry_year,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    const char* source)
{
    if (entry_year == 0u) {
        return 0;
    }
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    if (sang == NULL || !memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        append_logf("KBO military selection skipped source=%s year=%u reason=no_sang", source != NULL ? source : "", entry_year);
        return 0;
    }
    uint32_t sang_id = *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t sang_league_id = *(uint32_t*)(sang + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (sang_id == 0u || sang_league_id == 0u) {
        append_logf("KBO military selection skipped source=%s year=%u reason=bad_sang_ids team=%u league=%u", source != NULL ? source : "", entry_year, sang_id, sang_league_id);
        return 0;
    }

    enum { KBO_MILITARY_SANG_CAPACITY = 35, KBO_MILITARY_SANG_ANNUAL_INTAKE = 18 };
    int on_roster = kbo_count_players_on_team_with_military_days(sang_id);
    int slots = KBO_MILITARY_SANG_CAPACITY - on_roster;
    if (slots > KBO_MILITARY_SANG_ANNUAL_INTAKE) {
        slots = KBO_MILITARY_SANG_ANNUAL_INTAKE;
    }
    if (slots <= 0) {
        append_logf("KBO military selection skipped source=%s year=%u reason=sang_full active=%d", source != NULL ? source : "", entry_year, on_roster);
        return 0;
    }

    int routed = 0;
    int considered = 0;
    for (;;) {
        int best_index = -1;
        int best_score = INT32_MIN;
        LONG count = g_kbo_military_draft_candidate_count;
        if (count < 0) { count = 0; }
        if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
        for (LONG i = 0; i < count; i++) {
            KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
            if (candidate->player_id == 0u || candidate->entry_year != entry_year || candidate->selected != 0u) {
                continue;
            }
            considered++;
            uintptr_t player_ptr = candidate->player_ptr;
            if (!kbo_player_pointer_plausible(player_ptr)) {
                player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
                candidate->player_ptr = player_ptr;
            }
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                    || kbo_military_effective_days_left(player) <= 0
                    || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == sang_id) {
                continue;
            }
            int score = kbo_military_draft_candidate_score(player);
            if (score > best_score) {
                best_score = score;
                best_index = (int)i;
            }
        }
        if (best_index < 0 || routed >= slots) {
            break;
        }

        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[best_index];
        uint8_t* player = (uint8_t*)candidate->player_ptr;
        uint32_t original_team_id = candidate->original_team_id != 0u
            ? candidate->original_team_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t original_league_id = candidate->original_league_id != 0u
            ? candidate->original_league_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        int called_pre_change = 0;
        int called_register = 0;
        int called_attach = 0;
        kbo_assign_player_to_team_internal(
            player,
            sang,
            sang_league_id,
            0,
            &called_pre_change,
            &called_register,
            &called_attach);
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
        *(uint32_t*)(player + 0x5c) = original_team_id;
        if (original_league_id != 0u) {
            *(uint32_t*)(player + 0x4c) = original_league_id;
        }
        register_active_kbo_military_loan(
            candidate->player_id,
            (uintptr_t)player,
            original_team_id,
            original_league_id,
            sang_id,
            sang_league_id);
        clear_kbo_military_unavailable_flags(player);
        candidate->selected = 1u;
        if (news_entries != NULL && routed < max_news_entries) {
            KboMilitarySelectionNewsEntry* news_entry = &news_entries[routed];
            news_entry->player_id = candidate->player_id;
            news_entry->original_team_id = original_team_id;
            news_entry->score = best_score;
            news_entry->player_ptr = (uintptr_t)player;
        }
        routed++;
        append_logf(
            "KBO military selection drafted source=%s year=%u player_id=%u original_team=%u service_team=%u score=%d pre=%d register=%d attach=%d",
            source != NULL ? source : "",
            entry_year,
            candidate->player_id,
            original_team_id,
            sang_id,
            best_score,
            called_pre_change,
            called_register,
            called_attach);
    }

    append_logf(
        "KBO military selection processed source=%s year=%u queued=%d considered=%d routed=%d active_before=%d slots=%d",
        source != NULL ? source : "",
        entry_year,
        kbo_count_military_draft_candidates_for_year(entry_year),
        considered,
        routed,
        on_roster,
        slots);
    return routed;
}

static int run_kbo_custom_military_event(
    uintptr_t event_ptr,
    const char* event_name,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source)
{
    (void)event_ptr;
    int returned = tick_kbo_military_service_days("military_selection_event");
    KboMilitarySelectionNewsEntry news_entries[24] = {0};
    int routed = kbo_route_queued_military_draft_candidates(
        (uint16_t)event_year,
        news_entries,
        (int)(sizeof(news_entries) / sizeof(news_entries[0])),
        source);
    uint32_t event_yyyymmdd =
        event_year * 10000u + event_month * 100u + event_day;
    int news_created = 0;
    if (routed > 0) {
        news_created = kbo_emit_military_selection_news(
            event_yyyymmdd,
            news_entries,
            routed,
            source);
    }
    append_logf(
        "KBO military selection reached source=%s event=%s year=%u routed=%d returned=%d news=%d queued_left=%d",
        source != NULL ? source : "",
        event_name != NULL ? event_name : "",
        event_year,
        routed,
        returned,
        news_created,
        kbo_count_military_draft_candidates_for_year((uint16_t)event_year));
    return 1;
}

/* ---- Hook wrappers ---- */

__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr,
    uintptr_t original_func_ptr)
{
    OotpMilitaryServiceEntryFn original_func = (OotpMilitaryServiceEntryFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)player_ptr);
    }

    LONG log_index = InterlockedIncrement(&g_military_service_entry_log_count);
    if (log_index <= 120) {
        uint32_t player_id    = 0;
        uint32_t parent_team  = 0;
        uint32_t active_team  = 0;
        uint32_t cur_league   = 0;
        uint32_t loan_team    = 0;
        uint32_t loan_league  = 0;
        int32_t  days_left    = INT32_MIN;
        uint8_t  restricted   = 0;
        uint8_t  secondary    = 0;
        uint8_t  inj_active   = 0;
        uint8_t  mil_active   = 0;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* p = (uint8_t*)player_ptr;
            player_id   = *(uint32_t*)(p + OOTP27_PLAYER_ID_OFFSET);
            parent_team = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            active_team = *(uint32_t*)(p + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            cur_league  = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            loan_team   = *(uint32_t*)(p + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            loan_league = *(uint32_t*)(p + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
            days_left   = kbo_military_effective_days_left(p);
            restricted  = p[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
            secondary   = p[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
            inj_active  = p[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
            mil_active  = p[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        }
        append_logf(
            "KBO military service entry wrapper #%ld original=%p player=%p plausible=%d"
            " player_id=%u parent_team=%u active_team=%u cur_league=%u"
            " loan_team=%u loan_league=%u days_left=%d"
            " restricted=%u secondary=%u inj_active=%u mil_active=%u",
            log_index,
            (void*)original_func_ptr, (void*)player_ptr,
            kbo_player_pointer_plausible(player_ptr),
            player_id, parent_team, active_team, cur_league,
            loan_team, loan_league, days_left,
            (uint32_t)restricted, (uint32_t)secondary,
            (uint32_t)inj_active, (uint32_t)mil_active);
    }

    if (kbo_player_pointer_plausible(player_ptr)) {
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0
                && kbo_military_effective_days_left(player) > 0) {
            uint32_t cur_year = 0;
            uint32_t cur_month = 0;
            uint32_t cur_day   = 0;
            if (!kbo_current_date_is_valid(&cur_year, &cur_month, &cur_day)) {
                kbo_current_year_relaxed(&cur_year);
            }
            append_logf(
                "KBO military service entry deferred player=%p player_id=%u"
                " year=%u date=%04u-%02u-%02u days_left=%d",
                (void*)player_ptr,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                cur_year, cur_year, cur_month, cur_day,
                kbo_military_effective_days_left(player));
            uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            if (active_team_id != 0u) {
                original_team_id = active_team_id;
            }
            if (original_team_id != 0u && cur_year >= 1982u && cur_year <= 2300u) {
                kbo_queue_military_draft_candidate(
                    player_ptr,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    (uint16_t)cur_year,
                    original_team_id,
                    original_league_id,
                    "military_service_entry_wrapper");
            }
        }
    }
}

__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr,
    uintptr_t original_func_ptr)
{
    OotpMilitaryServiceEntryFn original_func = (OotpMilitaryServiceEntryFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)player_ptr);
    }

    kbo_flush_pending_foreign_priority_events("military_status_update_wrapper");

    if (!kbo_fix_enabled() || !kbo_player_is_registered_active_military_loan(player_ptr)) {
        return;
    }

    return_completed_kbo_military_loan_player(
        (uint8_t*)player_ptr, "military_status_update_wrapper", 0, 1);
}
