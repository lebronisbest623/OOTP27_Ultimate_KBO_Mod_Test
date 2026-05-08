/* Read-only opening-day salary snapshot for later KBO FA grade research. */

#define KBO_FA_SALARY_SNAPSHOT_THREAD_SLEEP_MS 1000u
#define KBO_FA_SALARY_SNAPSHOT_OPENING_WINDOW_DAYS 3u
#define KBO_FA_SALARY_SNAPSHOT_GRADE_MAX 4096
#define KBO_FA_SALARY_PHASE_EVENT_RING_SIZE 32u
#define KBO_FA_SALARY_PHASE_EVENT_RING_MASK (KBO_FA_SALARY_PHASE_EVENT_RING_SIZE - 1u)

typedef struct KboFaSalarySnapshotRow {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t ranking_team_id;
    uint32_t current_league_id;
    uint32_t draft_league_id;
    uint16_t age;
    uint8_t retired_flag;
    uint8_t contract_level;
    uint8_t foreign_flag;
    int32_t salary;
    int32_t contract_status;
    int32_t contract_start_year;
    int32_t contract_years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS];
    uint32_t overall_rank;
    uint32_t overall_ordinal;
    uint32_t team_rank;
    uint32_t team_ordinal;
    char player_name[96];
} KboFaSalarySnapshotRow;

typedef struct KboFaSalarySnapshotGrade {
    uint32_t player_id;
    uint32_t snapshot_date;
    uint32_t season;
    uint32_t opening_day;
    uint32_t ranking_team_id;
    uint8_t foreign_flag;
    int32_t salary;
    uint32_t overall_rank;
    uint32_t overall_ordinal;
    uint32_t team_rank;
    uint32_t team_ordinal;
    char grade[12];
} KboFaSalarySnapshotGrade;

static volatile LONG g_kbo_fa_salary_snapshot_thread_started = 0;
static volatile LONG g_kbo_fa_salary_snapshot_phase_event_thread_started = 0;
static volatile LONG g_kbo_fa_salary_snapshot_phase_event_write_cursor = 0;
static volatile LONG g_kbo_fa_salary_snapshot_phase_event_published_sequence = 0;
static volatile LONG g_kbo_fa_salary_snapshot_phase_event_consumed_sequence = 0;
static uintptr_t g_kbo_fa_salary_snapshot_phase_event_league_ptrs[KBO_FA_SALARY_PHASE_EVENT_RING_SIZE];
static uint32_t g_kbo_fa_salary_snapshot_phase_event_values[KBO_FA_SALARY_PHASE_EVENT_RING_SIZE];
static uint32_t g_kbo_fa_salary_snapshot_phase_event_site_rvas[KBO_FA_SALARY_PHASE_EVENT_RING_SIZE];
static volatile LONG g_kbo_fa_salary_snapshot_phase_pending_active = 0;
static uintptr_t g_kbo_fa_salary_snapshot_phase_pending_league_ptr = 0;
static volatile LONG g_kbo_fa_salary_snapshot_phase_pending_value = 0;
static volatile LONG g_kbo_fa_salary_snapshot_phase_pending_site_rva = 0;
static volatile LONG g_kbo_fa_salary_snapshot_cached_exists_season = 0;
static volatile LONG g_kbo_fa_salary_snapshot_cached_exists_value = -1;

static int kbo_fa_salary_snapshot_path(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || season < 1982u || season > 2200u) {
        return 0;
    }

    char file_name[96] = {0};
    snprintf(file_name, sizeof(file_name), "fa_salary_opening_day_snapshot_%04u.csv", season);
    return kbo_get_save_scoped_data_file(file_name, out, out_size);
}

static int kbo_fa_salary_snapshot_file_exists(uint32_t season)
{
    LONG cached_season = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, 0, 0);
    LONG cached_value = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, -1, -1);
    if (cached_season == (LONG)season && cached_value != -1) {
        return cached_value == 1;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, 0);
        return 0;
    }

    int exists = attrs.nFileSizeHigh != 0u || attrs.nFileSizeLow != 0u;
    InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, exists ? 1 : 0);
    return exists;
}

static uint32_t kbo_fa_salary_snapshot_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }

    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static uint32_t kbo_fa_salary_snapshot_resolve_ranking_team(uint32_t league_id, uint32_t current_team_id, uint32_t active_team_id)
{
    if (active_team_id != 0u && kbo_fa_salary_snapshot_team_league_id(active_team_id) == league_id) {
        return active_team_id;
    }
    if (current_team_id != 0u && kbo_fa_salary_snapshot_team_league_id(current_team_id) == league_id) {
        return current_team_id;
    }
    return 0u;
}

static int kbo_fa_salary_snapshot_read_opening_day(uintptr_t league_ptr, uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }
    if (league_ptr == 0
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                OOTP27_SEASON_START_DATE_SEC_OFFSET - OOTP27_SEASON_START_DATE_YEAR_OFFSET + sizeof(uint8_t))) {
        return 0;
    }

    uint32_t year = *(uint16_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint32_t day = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint32_t month = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    if (year < 1982u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    char scratch[16] = {0};
    if (!kbo_format_history_date(scratch, sizeof(scratch), year, month, day)) {
        return 0;
    }

    if (out_date != NULL) {
        *out_date = year * 10000u + month * 100u + day;
    }
    return 1;
}

static int kbo_fa_salary_snapshot_current_date_in_opening_window(uint32_t date, uint32_t opening_day)
{
    if (date == 0u || opening_day == 0u) {
        return 0;
    }

    uint32_t year = date / 10000u;
    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    uint32_t opening_year = opening_day / 10000u;
    uint32_t opening_month = (opening_day / 100u) % 100u;
    uint32_t opening_day_of_month = opening_day % 100u;
    uint32_t current_serial = kbo_date_serial(year, month, day);
    uint32_t opening_serial = kbo_date_serial(opening_year, opening_month, opening_day_of_month);
    if (current_serial == 0u || opening_serial == 0u || current_serial < opening_serial) {
        return 0;
    }
    return current_serial - opening_serial <= KBO_FA_SALARY_SNAPSHOT_OPENING_WINDOW_DAYS;
}

static int kbo_fa_salary_snapshot_read_uint_attr(const char* text, const char* name, uint32_t* out_value)
{
    if (text == NULL || name == NULL || out_value == NULL) {
        return 0;
    }

    const char* found = strstr(text, name);
    if (found == NULL) {
        return 0;
    }

    const char* equals = strchr(found, '=');
    if (equals == NULL) {
        return 0;
    }

    const char* p = equals + 1;
    while (*p == ' ' || *p == '\t' || *p == '"' || *p == '\'') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint32_t value = 0u;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        ++p;
    }

    *out_value = value;
    return 1;
}

static int kbo_fa_salary_snapshot_load_schedule_opening_day_from_file(const char* path, uint32_t season, uint32_t* out_opening_day)
{
    if (path == NULL || out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char text[8192] = {0};
    DWORD read = 0;
    BOOL ok = ReadFile(file, text, sizeof(text) - 1u, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0) {
        return 0;
    }
    text[read < sizeof(text) ? read : sizeof(text) - 1u] = '\0';

    uint32_t start_month = 0u;
    uint32_t start_day = 0u;
    if (!kbo_fa_salary_snapshot_read_uint_attr(text, "start_month", &start_month)
            || !kbo_fa_salary_snapshot_read_uint_attr(text, "start_day", &start_day)) {
        return 0;
    }
    if (start_month < 1u || start_month > 12u || start_day < 1u || start_day > 31u
            || kbo_date_serial(season, start_month, start_day) == 0u) {
        return 0;
    }

    *out_opening_day = season * 10000u + start_month * 100u + start_day;
    return 1;
}

static int kbo_fa_salary_snapshot_load_schedule_opening_day(uint32_t season, uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }

    char exe_path[MAX_PATH] = {0};
    DWORD got = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (got == 0 || got >= sizeof(exe_path)) {
        return 0;
    }

    char* slash = strrchr(exe_path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';

    const char* suffixes[] = { "", "_ap" };
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        char path[MAX_PATH] = {0};
        int written = snprintf(
            path,
            sizeof(path),
            "%s\\data\\schedules\\major_league_ml_c_%u%s.lsdl",
            exe_path,
            season,
            suffixes[i]);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            continue;
        }
        if (kbo_fa_salary_snapshot_load_schedule_opening_day_from_file(path, season, out_opening_day)) {
            return 1;
        }
    }

    return 0;
}

static int kbo_fa_salary_snapshot_file_contains_opening_day_message(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 16384u) {
        CloseHandle(file);
        return 0;
    }

    char* data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, size, &read, NULL);
    CloseHandle(file);
    int found = ok
        && read > 0
        && strstr(data, "Breaking KBO News: Opening Day") != NULL
        && strstr(data, "Korean Baseball Organization") != NULL;
    HeapFree(GetProcessHeap(), 0, data);
    return found;
}

static int kbo_fa_salary_snapshot_today_has_opening_day_message(uint32_t date)
{
    if (date == 0u) {
        return 0;
    }

    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    if (month < 3u || month > 4u || day < 1u || day > 31u) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char pattern[MAX_PATH] = {0};
    snprintf(pattern, sizeof(pattern), "%s\\messages\\message*.txt", save_path);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int found = 0;
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        char path[MAX_PATH] = {0};
        snprintf(path, sizeof(path), "%s\\messages\\%s", save_path, find_data.cFileName);
        if (kbo_fa_salary_snapshot_file_contains_opening_day_message(path)) {
            found = 1;
            break;
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    return found;
}

static int32_t kbo_fa_salary_snapshot_player_salary_for_season(uint8_t* player, uint32_t season)
{
    int32_t years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS] = {0};
    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }

    int32_t start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    if (start_year >= 1982 && start_year <= 2200 && season >= (uint32_t)start_year) {
        uint32_t index = season - (uint32_t)start_year;
        if (index < OOTP27_PLAYER_CONTRACT_SALARY_YEARS && years[index] > 0) {
            return years[index];
        }
    }

    if (years[0] > 0) {
        return years[0];
    }

    for (uint32_t i = 1; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        if (years[i] > 0) {
            return years[i];
        }
    }

    return 0;
}

static void kbo_fa_salary_snapshot_copy_contract_years(uint8_t* player, KboFaSalarySnapshotRow* row)
{
    if (player == NULL || row == NULL) {
        return;
    }

    row->contract_status = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
    row->contract_start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        row->contract_years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }
}

static int __cdecl kbo_fa_salary_snapshot_compare_overall(const void* a, const void* b)
{
    const KboFaSalarySnapshotRow* left = (const KboFaSalarySnapshotRow*)a;
    const KboFaSalarySnapshotRow* right = (const KboFaSalarySnapshotRow*)b;

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static int __cdecl kbo_fa_salary_snapshot_compare_team(const void* a, const void* b)
{
    const KboFaSalarySnapshotRow* left = (const KboFaSalarySnapshotRow*)a;
    const KboFaSalarySnapshotRow* right = (const KboFaSalarySnapshotRow*)b;

    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static void kbo_fa_salary_snapshot_assign_overall_ranks(KboFaSalarySnapshotRow* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_overall);

    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].overall_rank = 0u;
        rows[i].overall_ordinal = 0u;
        if (rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].overall_rank = current_rank;
        rows[i].overall_ordinal = ordinal;
    }
}

static void kbo_fa_salary_snapshot_assign_team_ranks(KboFaSalarySnapshotRow* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_team);

    uint32_t current_team = 0u;
    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].team_rank = 0u;
        rows[i].team_ordinal = 0u;
        if (rows[i].ranking_team_id == 0u || rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        if (rows[i].ranking_team_id != current_team) {
            current_team = rows[i].ranking_team_id;
            ordinal = 0u;
            current_rank = 0u;
            last_salary = -1;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].team_rank = current_rank;
        rows[i].team_ordinal = ordinal;
    }
}

static void kbo_fa_salary_snapshot_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        const char* p = text;
        while (*p != '\0') {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
            ++p;
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

static int kbo_fa_salary_snapshot_parse_csv_field(char** cursor, char* out, size_t out_size)
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

static uint32_t kbo_fa_salary_snapshot_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    char* tail = NULL;
    unsigned long value = strtoul(text, &tail, 10);
    if (tail == text || value > 0xfffffffful) {
        return 0u;
    }
    return (uint32_t)value;
}

static int32_t kbo_fa_salary_snapshot_parse_i32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    char* tail = NULL;
    long value = strtol(text, &tail, 10);
    if (tail == text) {
        return 0;
    }
    return (int32_t)value;
}

static const char* kbo_fa_salary_snapshot_grade_for_ranks(uint32_t overall_rank, uint32_t team_rank, int32_t salary)
{
    if (salary <= 0 || (overall_rank == 0u && team_rank == 0u)) {
        return "UNKNOWN";
    }
    if ((overall_rank != 0u && overall_rank <= 30u) || (team_rank != 0u && team_rank <= 3u)) {
        return "A";
    }
    if ((overall_rank != 0u && overall_rank <= 60u) || (team_rank != 0u && team_rank <= 10u)) {
        return "B";
    }
    return "C";
}

static int __cdecl kbo_fa_salary_snapshot_grade_compare_overall(const void* a, const void* b)
{
    const KboFaSalarySnapshotGrade* left = (const KboFaSalarySnapshotGrade*)a;
    const KboFaSalarySnapshotGrade* right = (const KboFaSalarySnapshotGrade*)b;

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static int __cdecl kbo_fa_salary_snapshot_grade_compare_team(const void* a, const void* b)
{
    const KboFaSalarySnapshotGrade* left = (const KboFaSalarySnapshotGrade*)a;
    const KboFaSalarySnapshotGrade* right = (const KboFaSalarySnapshotGrade*)b;

    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static void kbo_fa_salary_snapshot_assign_grade_overall_ranks(KboFaSalarySnapshotGrade* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotGrade), kbo_fa_salary_snapshot_grade_compare_overall);

    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].overall_rank = 0u;
        rows[i].overall_ordinal = 0u;
        if (rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].overall_rank = current_rank;
        rows[i].overall_ordinal = ordinal;
    }
}

static void kbo_fa_salary_snapshot_assign_grade_team_ranks(KboFaSalarySnapshotGrade* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotGrade), kbo_fa_salary_snapshot_grade_compare_team);

    uint32_t current_team = 0u;
    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].team_rank = 0u;
        rows[i].team_ordinal = 0u;
        if (rows[i].ranking_team_id == 0u || rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        if (rows[i].ranking_team_id != current_team) {
            current_team = rows[i].ranking_team_id;
            ordinal = 0u;
            current_rank = 0u;
            last_salary = -1;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].team_rank = current_rank;
        rows[i].team_ordinal = ordinal;
    }
}

static int kbo_fa_salary_snapshot_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0 || season < 1982u || season > 2200u) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
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
    if (size == INVALID_FILE_SIZE || size == 0u || size > 8u * 1024u * 1024u) {
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
            KboFaSalarySnapshotGrade grade_row;
            memset(&grade_row, 0, sizeof(grade_row));
            for (int field_index = 0; field_index <= 21 && *p != '\0'; field_index++) {
                char field[128] = {0};
                if (!kbo_fa_salary_snapshot_parse_csv_field(&p, field, sizeof(field))) {
                    break;
                }

                switch (field_index) {
                case 0: grade_row.snapshot_date = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 1: grade_row.season = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 2: grade_row.opening_day = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 5: grade_row.player_id = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 10: grade_row.ranking_team_id = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 16: grade_row.foreign_flag = kbo_fa_salary_snapshot_parse_u32(field) != 0u ? 1u : 0u; break;
                case 17: grade_row.salary = kbo_fa_salary_snapshot_parse_i32(field); break;
                case 18: grade_row.overall_rank = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 19: grade_row.overall_ordinal = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 20: grade_row.team_rank = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 21: grade_row.team_ordinal = kbo_fa_salary_snapshot_parse_u32(field); break;
                default: break;
                }
            }

            if (grade_row.player_id != 0u && grade_row.season == season) {
                rows[count++] = grade_row;
            }
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    kbo_fa_salary_snapshot_assign_grade_overall_ranks(rows, count);
    kbo_fa_salary_snapshot_assign_grade_team_ranks(rows, count);
    for (int i = 0; i < count; i++) {
        snprintf(
            rows[i].grade,
            sizeof(rows[i].grade),
            "%s",
            kbo_fa_salary_snapshot_grade_for_ranks(
                rows[i].overall_rank,
                rows[i].team_rank,
                rows[i].salary));
    }
    return count;
}

static const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id)
{
    if (rows == NULL || row_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id == player_id) {
            return &rows[i];
        }
    }
    return NULL;
}

static int kbo_fa_salary_snapshot_write_csv(
    const KboFaSalarySnapshotRow* rows,
    int row_count,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        append_logf("KBO FA salary snapshot skipped source=%s reason=path_unavailable season=%u", source != NULL ? source : "", season);
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA salary snapshot failed open path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "date,season,opening_day,source,league_id,player_id,name,nation_id,current_team_id,active_team_id,ranking_team_id,current_league_id,draft_league_id,age,retired_flag,contract_level,foreign_flag,salary,overall_rank,overall_ordinal,team_rank,team_ordinal,contract_status,contract_start_year,contract_y1,contract_y2,contract_y3,contract_y4,contract_y5,contract_y6,contract_y7,contract_y8,contract_y9,contract_y10\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < row_count; i++) {
        const KboFaSalarySnapshotRow* row = &rows[i];
        char prefix[512] = {0};
        int prefix_len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,",
            date,
            season,
            opening_day);
        if (prefix_len > 0) {
            WriteFile(file, prefix, (DWORD)prefix_len, &written, NULL);
        }
        kbo_fa_salary_snapshot_write_csv_text(file, source != NULL ? source : "");
        snprintf(
            prefix,
            sizeof(prefix),
            ",%u,%u,",
            league_id,
            row->player_id);
        WriteFile(file, prefix, (DWORD)strlen(prefix), &written, NULL);
        kbo_fa_salary_snapshot_write_csv_text(file, row->player_name);
        snprintf(
            prefix,
            sizeof(prefix),
            ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%d,%d",
            row->nation_id,
            row->current_team_id,
            row->active_team_id,
            row->ranking_team_id,
            row->current_league_id,
            row->draft_league_id,
            (uint32_t)row->age,
            (uint32_t)row->retired_flag,
            (uint32_t)row->contract_level,
            (uint32_t)row->foreign_flag,
            row->salary,
            row->overall_rank,
            row->overall_ordinal,
            row->team_rank,
            row->team_ordinal,
            row->contract_status,
            row->contract_start_year);
        WriteFile(file, prefix, (DWORD)strlen(prefix), &written, NULL);
        for (uint32_t y = 0; y < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; y++) {
            char salary_text[32] = {0};
            snprintf(salary_text, sizeof(salary_text), ",%d", row->contract_years[y]);
            WriteFile(file, salary_text, (DWORD)strlen(salary_text), &written, NULL);
        }
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    CloseHandle(file);
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}

static int kbo_capture_fa_salary_opening_day_snapshot(const char* source, uint32_t date, uint32_t season, uint32_t opening_day, uint32_t league_id)
{
    KBO_PROFILE_BEGIN(profile_snapshot_capture);
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_logf("KBO FA salary snapshot skipped source=%s reason=no_player_vector date=%u", source != NULL ? source : "", date);
        KBO_PROFILE_END(profile_snapshot_capture, "fa_salary_snapshot.capture.no_player_vector");
        return 0;
    }

    KboFaSalarySnapshotRow* rows = (KboFaSalarySnapshotRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)player_count * sizeof(KboFaSalarySnapshotRow));
    if (rows == NULL) {
        append_logf("KBO FA salary snapshot skipped source=%s reason=alloc_failed count=%d", source != NULL ? source : "", player_count);
        KBO_PROFILE_END(profile_snapshot_capture, "fa_salary_snapshot.capture.alloc_failed");
        return 0;
    }

    int row_count = 0;
    int scanned = 0;
    int salary_rows = 0;
    int zero_salary_rows = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t ranking_team_id = kbo_fa_salary_snapshot_resolve_ranking_team(league_id, current_team_id, active_team_id);
        if (ranking_team_id == 0u) {
            continue;
        }

        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        if (retired_flag != 0u || age < 16u || age > 60u) {
            continue;
        }

        KboFaSalarySnapshotRow* row = &rows[row_count++];
        row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row->current_team_id = current_team_id;
        row->active_team_id = active_team_id;
        row->ranking_team_id = ranking_team_id;
        row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        row->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        row->age = age;
        row->retired_flag = retired_flag;
        row->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        row->foreign_flag = row->nation_id != OOTP27_KBO_KOREA_NATION_ID ? 1u : 0u;
        row->salary = kbo_fa_salary_snapshot_player_salary_for_season(player, season);
        kbo_fa_salary_snapshot_copy_contract_years(player, row);
        kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
        if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
            snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
        }

        if (row->salary > 0) {
            salary_rows++;
        } else {
            zero_salary_rows++;
        }
    }

    kbo_fa_salary_snapshot_assign_overall_ranks(rows, row_count);
    kbo_fa_salary_snapshot_assign_team_ranks(rows, row_count);
    qsort(rows, (size_t)row_count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_overall);

    char path[MAX_PATH] = {0};
    int wrote = kbo_fa_salary_snapshot_write_csv(rows, row_count, date, season, opening_day, league_id, source, path, sizeof(path));
    if (wrote) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, 1);
        append_logf(
            "KBO FA salary opening-day snapshot written source=%s date=%u season=%u opening_day=%u league=%u scanned=%d rows=%d salary_rows=%d zero_salary_rows=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            opening_day,
            league_id,
            scanned,
            row_count,
            salary_rows,
            zero_salary_rows,
            path);
    }

    HeapFree(GetProcessHeap(), 0, rows);
    KBO_PROFILE_END(profile_snapshot_capture, "fa_salary_snapshot.capture");
    return wrote;
}

static int kbo_fa_salary_snapshot_phase_event_league_is_kbo(uintptr_t league_ptr, uint32_t league_id)
{
    if (league_ptr == 0 || league_id == 0
            || !memory_range_readable((void*)league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)) {
        return 0;
    }

    uint32_t legacy_id = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET);
    uint32_t alternate_id = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    if (legacy_id == league_id
            && kbo_league_candidate_matches_id(league_ptr, league_id, OOTP27_KBO_LEAGUE_ID_OFFSET)) {
        return 1;
    }
    if (alternate_id == league_id
            && kbo_league_candidate_matches_id(league_ptr, league_id, OOTP27_KBO_LEAGUE_ID_OFFSET + 8u)) {
        return 1;
    }
    return 0;
}

static void kbo_fa_salary_snapshot_set_pending_phase_event(uintptr_t league_ptr, uint32_t value, uint32_t site_rva)
{
    g_kbo_fa_salary_snapshot_phase_pending_league_ptr = league_ptr;
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_value, (LONG)value);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_site_rva, (LONG)site_rva);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 1);
    append_logf(
        "KBO FA salary opening-day phase hook observed site=0x%x league=%p value=%u",
        site_rva,
        (void*)league_ptr,
        value);
}

static void kbo_fa_salary_snapshot_drain_phase_events_once(void)
{
    LONG latest = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_event_published_sequence, 0, 0);
    LONG consumed = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_event_consumed_sequence, 0, 0);
    if (latest == consumed) {
        return;
    }

    if (latest - consumed > (LONG)KBO_FA_SALARY_PHASE_EVENT_RING_SIZE) {
        append_logf(
            "KBO FA salary opening-day phase hook event ring overflow consumed=%ld latest=%ld",
            (long)consumed,
            (long)latest);
        consumed = latest - (LONG)KBO_FA_SALARY_PHASE_EVENT_RING_SIZE;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    for (LONG event_no = consumed; event_no < latest; event_no++) {
        uint32_t index = (uint32_t)event_no & KBO_FA_SALARY_PHASE_EVENT_RING_MASK;
        uintptr_t league_ptr = g_kbo_fa_salary_snapshot_phase_event_league_ptrs[index];
        uint32_t value = g_kbo_fa_salary_snapshot_phase_event_values[index];
        uint32_t site_rva = g_kbo_fa_salary_snapshot_phase_event_site_rvas[index];
        if (value != 3u || !kbo_fa_salary_snapshot_phase_event_league_is_kbo(league_ptr, league_id)) {
            continue;
        }
        kbo_fa_salary_snapshot_set_pending_phase_event(league_ptr, value, site_rva);
    }

    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_event_consumed_sequence, latest);
}

static void kbo_fa_salary_snapshot_try_pending_phase_event(void)
{
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 1, 1) == 0) {
        return;
    }

    uintptr_t league_ptr = g_kbo_fa_salary_snapshot_phase_pending_league_ptr;
    uint32_t value = (uint32_t)InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_pending_value, 0, 0);
    uint32_t site_rva = (uint32_t)InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_pending_site_rva, 0, 0);
    if (value != 3u) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (!kbo_fa_salary_snapshot_phase_event_league_is_kbo(league_ptr, league_id)) {
        append_logf(
            "KBO FA salary opening-day phase hook ignored site=0x%x league=%p value=%u reason=not_kbo_league",
            site_rva,
            (void*)league_ptr,
            value);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    uint32_t opening_day = 0u;
    if (!kbo_fa_salary_snapshot_read_opening_day(league_ptr, &opening_day)) {
        return;
    }

    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return;
    }

    uint32_t date = year * 10000u + month * 100u + day;
    if (date < opening_day) {
        return;
    }

    uint32_t season = opening_day / 10000u;
    if (season < 1982u || season > 2200u) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    if (kbo_fa_salary_snapshot_file_exists(season)) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    if (date > opening_day) {
        append_logf(
            "KBO FA salary opening-day phase hook missed exact opening day site=0x%x date=%u opening_day=%u league=%u",
            site_rva,
            date,
            opening_day,
            league_id);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    char source[64] = {0};
    snprintf(source, sizeof(source), "season_phase_hook_0x%x", site_rva);
    if (kbo_capture_fa_salary_opening_day_snapshot(source, date, season, opening_day, league_id)) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
    }
}

static DWORD WINAPI kbo_fa_salary_snapshot_phase_event_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO FA salary opening-day phase hook event thread started");

    for (;;) {
        Sleep(250);
        KBO_PROFILE_BEGIN(profile_phase_thread_tick);
        if (!kbo_fix_enabled()) {
            KBO_PROFILE_END(profile_phase_thread_tick, "fa_salary_snapshot.phase_thread.disabled_tick");
            continue;
        }

        kbo_fa_salary_snapshot_drain_phase_events_once();
        kbo_fa_salary_snapshot_try_pending_phase_event();
        KBO_PROFILE_END(profile_phase_thread_tick, "fa_salary_snapshot.phase_thread.tick");
    }

    return 0;
}

static void start_kbo_fa_salary_snapshot_phase_event_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_event_thread_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_fa_salary_snapshot_phase_event_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_event_thread_started, 0);
        append_logf("KBO FA salary opening-day phase hook event thread failed to start gle=%lu", GetLastError());
    }
}

static DWORD WINAPI kbo_fa_salary_snapshot_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO FA salary opening-day snapshot thread started");

    uint32_t last_log_date = 0u;
    uint32_t last_log_opening_day = 0u;
    uint32_t captured_season = 0u;
    uint32_t cached_schedule_year = 0u;
    uint32_t cached_schedule_opening_day = 0u;
    int cached_schedule_found = 0;
    DWORD cached_schedule_checked_ms = 0u;
    uint32_t cached_message_date = 0u;
    int cached_message_found = 0;
    DWORD cached_message_checked_ms = 0u;
    uint32_t quiet_opening_unavailable_year = 0u;
    for (;;) {
        Sleep(KBO_FA_SALARY_SNAPSHOT_THREAD_SLEEP_MS);
        KBO_PROFILE_BEGIN(profile_snapshot_thread_tick);
        if (!kbo_fix_enabled()) {
            KBO_PROFILE_END(profile_snapshot_thread_tick, "fa_salary_snapshot.thread.disabled_tick");
            continue;
        }

        uint32_t year = 0;
        uint32_t month = 0;
        uint32_t day = 0;
        if (!kbo_current_date_is_valid(&year, &month, &day)) {
            KBO_PROFILE_END(profile_snapshot_thread_tick, "fa_salary_snapshot.thread.no_date");
            continue;
        }
        uint32_t date = year * 10000u + month * 100u + day;

        uint32_t league_id = kbo_resolve_kbo_league_id();
        uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
        uint32_t opening_day = 0u;
        if (league_ptr == 0 || !kbo_fa_salary_snapshot_read_opening_day(league_ptr, &opening_day)) {
            DWORD now_ms = GetTickCount();
            int schedule_found = cached_schedule_year == year ? cached_schedule_found : 0;
            if (cached_schedule_year != year
                    || (!cached_schedule_found && (cached_schedule_checked_ms == 0u || now_ms - cached_schedule_checked_ms >= 30000u))) {
                cached_schedule_year = year;
                cached_schedule_opening_day = 0u;
                cached_schedule_checked_ms = now_ms;
                cached_schedule_found = kbo_fa_salary_snapshot_load_schedule_opening_day(year, &cached_schedule_opening_day);
                schedule_found = cached_schedule_found;
            }
            if (schedule_found) {
                opening_day = cached_schedule_opening_day;
            }

            int message_found = 0;
            if (!schedule_found) {
                if (cached_message_date != date
                        && (cached_message_checked_ms == 0u || now_ms - cached_message_checked_ms >= 30000u)) {
                    cached_message_date = date;
                    cached_message_checked_ms = now_ms;
                    cached_message_found = kbo_fa_salary_snapshot_today_has_opening_day_message(date);
                }
                message_found = cached_message_found;
            }

            if (!schedule_found && !message_found) {
                if (quiet_opening_unavailable_year != year && date != last_log_date) {
                    append_logf("KBO FA salary snapshot waiting date=%u league=%u reason=opening_day_unavailable", date, league_id);
                    last_log_date = date;
                    if (month >= 5u) {
                        quiet_opening_unavailable_year = year;
                    }
                }
                KBO_PROFILE_END(profile_snapshot_thread_tick, "fa_salary_snapshot.thread.opening_day_unavailable");
                continue;
            }
            if (opening_day == 0u) {
                opening_day = date;
            }
        }

        if (!kbo_fa_salary_snapshot_current_date_in_opening_window(date, opening_day)) {
            if (date != last_log_date || opening_day != last_log_opening_day) {
                append_logf(
                    "KBO FA salary snapshot waiting date=%u opening_day=%u league=%u",
                    date,
                    opening_day,
                    league_id);
                last_log_date = date;
                last_log_opening_day = opening_day;
            }
            KBO_PROFILE_END(profile_snapshot_thread_tick, "fa_salary_snapshot.thread.outside_opening_window");
            continue;
        }

        if (captured_season == year || kbo_fa_salary_snapshot_file_exists(year)) {
            captured_season = year;
            KBO_PROFILE_END(profile_snapshot_thread_tick, "fa_salary_snapshot.thread.already_captured");
            continue;
        }

        if (kbo_capture_fa_salary_opening_day_snapshot("opening_day_thread", date, year, opening_day, league_id)) {
            captured_season = year;
        }
        KBO_PROFILE_END(profile_snapshot_thread_tick, "fa_salary_snapshot.thread.capture_attempt");
    }

    return 0;
}

static void start_kbo_fa_salary_snapshot_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_thread_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_fa_salary_snapshot_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_thread_started, 0);
        append_logf("KBO FA salary opening-day snapshot thread failed to start gle=%lu", GetLastError());
    }
}
