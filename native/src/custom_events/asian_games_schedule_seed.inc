/* Asian Games schedule seed loading. Included from native/src/custom_events.inc. */

#define KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX 96

typedef struct KboAsianGamesScheduleSeed {
    uint32_t year;
    char host_city[48];
    char host_country[48];
    char status[24];
    uint32_t tournament_start;
    uint32_t tournament_end;
    uint32_t selection_date;
    uint32_t departure_date;
    uint32_t final_date;
    uint8_t auto_schedule;
    char notes[128];
} KboAsianGamesScheduleSeed;

static KboAsianGamesScheduleSeed g_kbo_asian_games_schedule_seeds[KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX];
static int g_kbo_asian_games_schedule_seed_count = 0;
static LONG g_kbo_asian_games_schedule_seed_lock = 0;
static LONG g_kbo_asian_games_schedule_seed_loaded = 0;
static char g_kbo_asian_games_schedule_seed_loaded_key[MAX_PATH * 3] = {0};

static void kbo_lock_asian_games_schedule_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_asian_games_schedule_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_asian_games_schedule_seeds(void)
{
    InterlockedExchange(&g_kbo_asian_games_schedule_seed_lock, 0);
}

static int kbo_get_save_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("asian_games_schedule_seed.csv", out, out_size);
}

static int kbo_get_global_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\asian_games_schedule_seed.csv", local_app_data);
    return out[0] != '\0';
}

static void kbo_asian_games_schedule_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }

    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1u);
    }

    size_t len = strlen(value);
    while (len > 0u) {
        char c = value[len - 1u];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '"') {
            break;
        }
        value[--len] = '\0';
    }
}

static void kbo_asian_games_schedule_read_cell(const char** cursor, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (cursor == NULL || *cursor == NULL) {
        return;
    }

    const char* p = *cursor;
    size_t len = 0u;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }

    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (len + 1u < out_size) {
                        out[len++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                if (*p == ',') {
                    p++;
                }
                break;
            }
        } else if (*p == ',') {
            p++;
            break;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }

        if (len + 1u < out_size) {
            out[len++] = *p;
        }
        p++;
    }

    out[len] = '\0';
    kbo_asian_games_schedule_trim_cell(out);
    *cursor = p;
}

static void kbo_asian_games_schedule_copy_text(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, value, len);
    out[len] = '\0';
}

static uint32_t kbo_asian_games_schedule_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    unsigned long long value = 0ull;
    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0u;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10ull + (unsigned long long)(*p - '0');
        if (value > UINT32_MAX) {
            return 0u;
        }
        p++;
    }
    return (uint32_t)value;
}

static uint32_t kbo_asian_games_schedule_parse_date(const char* text)
{
    if (text == NULL || text[0] == '\0'
            || ascii_equals_ignore_case(text, "TBD")
            || ascii_equals_ignore_case(text, "NA")
            || ascii_equals_ignore_case(text, "N/A")) {
        return 0u;
    }

    uint32_t date = 0u;
    if (!kbo_parse_yyyymmdd(text, &date)) {
        return 0u;
    }
    if (kbo_date_serial(date / 10000u, (date / 100u) % 100u, date % 100u) == 0u) {
        return 0u;
    }
    return date;
}

static uint8_t kbo_asian_games_schedule_parse_auto(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    return (uint8_t)(ascii_equals_ignore_case(text, "1")
        || ascii_equals_ignore_case(text, "true")
        || ascii_equals_ignore_case(text, "yes")
        || ascii_equals_ignore_case(text, "auto")
        || ascii_equals_ignore_case(text, "schedule"));
}

static int kbo_parse_asian_games_schedule_seed_line(const char* line, KboAsianGamesScheduleSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[512] = {0};
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

    const char* cursor = p;
    char fields[11][128];
    memset(fields, 0, sizeof(fields));
    for (int i = 0; i < 11; i++) {
        kbo_asian_games_schedule_read_cell(&cursor, fields[i], sizeof(fields[i]));
    }

    if (fields[0][0] == '\0'
            || ascii_equals_ignore_case(fields[0], "year")
            || ascii_equals_ignore_case(fields[0], "event_year")) {
        return 0;
    }

    out->year = kbo_asian_games_schedule_parse_u32(fields[0]);
    if (out->year < 1982u || out->year > 2200u) {
        return 0;
    }
    kbo_asian_games_schedule_copy_text(out->host_city, sizeof(out->host_city), fields[1]);
    kbo_asian_games_schedule_copy_text(out->host_country, sizeof(out->host_country), fields[2]);
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status), fields[3][0] != '\0' ? fields[3] : "official");
    out->tournament_start = kbo_asian_games_schedule_parse_date(fields[4]);
    out->tournament_end = kbo_asian_games_schedule_parse_date(fields[5]);
    out->selection_date = kbo_asian_games_schedule_parse_date(fields[6]);
    out->departure_date = kbo_asian_games_schedule_parse_date(fields[7]);
    out->final_date = kbo_asian_games_schedule_parse_date(fields[8]);
    out->auto_schedule = kbo_asian_games_schedule_parse_auto(fields[9]);
    kbo_asian_games_schedule_copy_text(out->notes, sizeof(out->notes), fields[10]);
    return 1;
}

static int kbo_add_asian_games_schedule_seed_locked(const KboAsianGamesScheduleSeed* seed)
{
    if (seed == NULL || seed->year == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (g_kbo_asian_games_schedule_seeds[i].year == seed->year) {
            g_kbo_asian_games_schedule_seeds[i] = *seed;
            return 0;
        }
    }
    if (g_kbo_asian_games_schedule_seed_count >= KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX) {
        return 0;
    }
    g_kbo_asian_games_schedule_seeds[g_kbo_asian_games_schedule_seed_count++] = *seed;
    return 1;
}

static void kbo_add_builtin_asian_games_schedule_seed_locked(
    uint32_t year,
    const char* host_city,
    const char* host_country,
    const char* status,
    uint32_t tournament_start,
    uint32_t tournament_end,
    uint32_t selection_date,
    uint32_t departure_date,
    uint32_t final_date,
    const char* notes)
{
    KboAsianGamesScheduleSeed seed;
    memset(&seed, 0, sizeof(seed));
    seed.year = year;
    kbo_asian_games_schedule_copy_text(seed.host_city, sizeof(seed.host_city), host_city);
    kbo_asian_games_schedule_copy_text(seed.host_country, sizeof(seed.host_country), host_country);
    kbo_asian_games_schedule_copy_text(seed.status, sizeof(seed.status), status);
    seed.tournament_start = tournament_start;
    seed.tournament_end = tournament_end;
    seed.selection_date = selection_date;
    seed.departure_date = departure_date;
    seed.final_date = final_date;
    seed.auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(seed.notes, sizeof(seed.notes), notes);
    kbo_add_asian_games_schedule_seed_locked(&seed);
}

static void kbo_add_builtin_asian_games_schedule_seeds_locked(void)
{
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2026u,
        "Aichi-Nagoya",
        "Japan",
        "official",
        20260919u,
        20261004u,
        20260805u,
        20260919u,
        20261004u,
        "Built-in Aichi-Nagoya 2026 Asian Games schedule.");
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2031u,
        "Doha",
        "Qatar",
        "provisional",
        20311104u,
        20311119u,
        20310920u,
        20311104u,
        20311119u,
        "Built-in provisional Doha 2031 Asian Games schedule.");
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2035u,
        "Riyadh",
        "Saudi Arabia",
        "provisional",
        20351129u,
        20351214u,
        20351015u,
        20351129u,
        20351214u,
        "Built-in provisional Riyadh 2035 Asian Games schedule.");
}

static int kbo_asian_games_year_is_projected(uint32_t year)
{
    return year >= 2039u && year <= 2200u && ((year - 2039u) % 4u) == 0u;
}

static uint32_t kbo_next_projected_asian_games_year(uint32_t from_year)
{
    if (from_year > 2200u) {
        return 0u;
    }
    if (from_year <= 2039u) {
        return 2039u;
    }
    uint32_t delta = from_year - 2039u;
    uint32_t year = 2039u + ((delta + 3u) / 4u) * 4u;
    return year <= 2200u ? year : 0u;
}

static void kbo_build_projected_asian_games_schedule(uint32_t year, KboAsianGamesScheduleSeed* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!kbo_asian_games_year_is_projected(year)) {
        return;
    }
    out->year = year;
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status), "projected");
    out->tournament_start = year * 10000u + 919u;
    out->tournament_end = year * 10000u + 1004u;
    out->selection_date = year * 10000u + 805u;
    out->departure_date = out->tournament_start;
    out->final_date = out->tournament_end;
    out->auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(
        out->notes,
        sizeof(out->notes),
        "Projected post-2035 Asian Games schedule; override this year in asian_games_schedule_seed.csv when official dates are known.");
}

static int kbo_import_asian_games_schedule_seed_file_locked(const char* path, const char* source)
{
    if (path == NULL || path[0] == '\0') {
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
    int imported = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[512] = {0};
            size_t line_len = (size_t)(next - cursor);
            while (line_len > 0u && (cursor[line_len - 1u] == '\r' || cursor[line_len - 1u] == '\n')) {
                line_len--;
            }
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, line_len);

            KboAsianGamesScheduleSeed seed;
            if (kbo_parse_asian_games_schedule_seed_line(line, &seed)) {
                kbo_add_asian_games_schedule_seed_locked(&seed);
                imported++;
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (imported > 0) {
        append_logf(
            "KBO Asian Games schedule seed import source=%s imported=%d path=%s",
            source != NULL ? source : "",
            imported,
            path);
    }
    return imported;
}

static void kbo_ensure_asian_games_schedule_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    kbo_get_global_asian_games_schedule_seed_path(global_seed_path, sizeof(global_seed_path));
    kbo_get_save_asian_games_schedule_seed_path(save_seed_path, sizeof(save_seed_path));

    char loaded_key[MAX_PATH * 3] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s", global_seed_path, save_seed_path);

    kbo_lock_asian_games_schedule_seeds();
    if (InterlockedCompareExchange(&g_kbo_asian_games_schedule_seed_loaded, 1, 0) == 0
            || _stricmp(g_kbo_asian_games_schedule_seed_loaded_key, loaded_key) != 0) {
        memset(g_kbo_asian_games_schedule_seeds, 0, sizeof(g_kbo_asian_games_schedule_seeds));
        g_kbo_asian_games_schedule_seed_count = 0;
        snprintf(g_kbo_asian_games_schedule_seed_loaded_key, sizeof(g_kbo_asian_games_schedule_seed_loaded_key), "%s", loaded_key);
        kbo_add_builtin_asian_games_schedule_seeds_locked();
        kbo_import_asian_games_schedule_seed_file_locked(global_seed_path, "global_seed");
        kbo_import_asian_games_schedule_seed_file_locked(save_seed_path, "save_seed");
    }
    kbo_unlock_asian_games_schedule_seeds();
}

static int kbo_get_asian_games_schedule_for_year(uint32_t year, KboAsianGamesScheduleSeed* out)
{
    if (year < 1982u || year > 2200u) {
        return 0;
    }
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }

    kbo_ensure_asian_games_schedule_seeds_loaded();
    kbo_lock_asian_games_schedule_seeds();
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (g_kbo_asian_games_schedule_seeds[i].year == year) {
            if (out != NULL) {
                *out = g_kbo_asian_games_schedule_seeds[i];
            }
            kbo_unlock_asian_games_schedule_seeds();
            return 1;
        }
    }
    kbo_unlock_asian_games_schedule_seeds();
    if (kbo_asian_games_year_is_projected(year)) {
        if (out != NULL) {
            kbo_build_projected_asian_games_schedule(year, out);
        }
        return 1;
    }
    return 0;
}

static int kbo_get_next_asian_games_schedule(uint32_t from_year, KboAsianGamesScheduleSeed* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (from_year < 1982u) {
        from_year = 1982u;
    }
    if (from_year > 2200u) {
        return 0;
    }

    kbo_ensure_asian_games_schedule_seeds_loaded();
    kbo_lock_asian_games_schedule_seeds();
    int best_index = -1;
    uint32_t best_year = 0u;
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        uint32_t year = g_kbo_asian_games_schedule_seeds[i].year;
        if (year < from_year) {
            continue;
        }
        if (best_index < 0 || year < best_year) {
            best_index = i;
            best_year = year;
        }
    }
    if (best_index >= 0) {
        uint32_t projected_year = kbo_next_projected_asian_games_year(from_year);
        if (projected_year != 0u && projected_year < best_year) {
            if (out != NULL) {
                kbo_build_projected_asian_games_schedule(projected_year, out);
            }
            kbo_unlock_asian_games_schedule_seeds();
            return 1;
        }
        if (out != NULL) {
            *out = g_kbo_asian_games_schedule_seeds[best_index];
        }
        kbo_unlock_asian_games_schedule_seeds();
        return 1;
    }
    kbo_unlock_asian_games_schedule_seeds();
    uint32_t projected_year = kbo_next_projected_asian_games_year(from_year);
    if (projected_year != 0u) {
        if (out != NULL) {
            kbo_build_projected_asian_games_schedule(projected_year, out);
        }
        return 1;
    }
    return 0;
}

static int kbo_asian_games_schedule_has_event_dates(const KboAsianGamesScheduleSeed* schedule)
{
    return schedule != NULL
        && schedule->selection_date != 0u
        && schedule->departure_date != 0u
        && schedule->final_date != 0u;
}

static int kbo_asian_games_schedule_auto_events_enabled(const KboAsianGamesScheduleSeed* schedule)
{
    return schedule != NULL
        && schedule->auto_schedule != 0u
        && kbo_asian_games_schedule_has_event_dates(schedule);
}

static const char* kbo_asian_games_schedule_status_label(const KboAsianGamesScheduleSeed* schedule)
{
    if (schedule == NULL || schedule->status[0] == '\0') {
        return "Seeded";
    }
    if (ascii_equals_ignore_case(schedule->status, "official")) {
        return "Official";
    }
    if (ascii_equals_ignore_case(schedule->status, "confirmed")) {
        return "Confirmed";
    }
    if (ascii_equals_ignore_case(schedule->status, "provisional")) {
        return "Provisional";
    }
    if (ascii_equals_ignore_case(schedule->status, "projected")) {
        return "Projected";
    }
    return schedule->status;
}
