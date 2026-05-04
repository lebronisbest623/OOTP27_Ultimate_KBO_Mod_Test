#include <stdint.h>

/* ---- Foreign waiver intelligence scaffold ---- */

static uint32_t read_u32_leading_number_from_file(const char* filename);
static int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size);
static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size);
static int kbo_load_foreign_waiver_rights(void);
static int kbo_persist_foreign_waiver_rights(void);
static void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd);
static int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value);

static int kbo_foreign_waiver_ai_enabled(void)
{
    return read_kbo_localappdata_flag_file("enable_foreign_waiver_ai.txt");
}

static uint32_t kbo_get_foreign_waiver_league_id(void)
{
    static uint32_t cached_league_id = 0u;
    if (cached_league_id == 0u) {
        cached_league_id = read_u32_leading_number_from_file("foreign_waiver_league_id.txt");
        if (cached_league_id == 0u) {
            cached_league_id = kbo_resolve_kbo_league_id();
        }
        if (cached_league_id != 0u) {
            append_logf("foreign waiver: resolved league id=%u", cached_league_id);
        }
    }
    return cached_league_id;
}

static int get_kbo_foreign_waiver_event_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_waiver_negotiation_window.txt", local_app_data);
    return 1;
}

/* ---- Korean foreign waiver negotiation window control ----
 * Preferred mode: derive from season-end transition and open for 20 days after.
 * File mode is fallback/override: if foreign_waiver_negotiation_window.txt exists
 * with valid YYYYMMDD,YYYYMMDD, that exact window is used.
 */
static uint32_t g_kbo_foreign_waiver_window_start_serial = 0;
static uint32_t g_kbo_foreign_waiver_window_end_serial = 0;
static uint32_t g_kbo_foreign_waiver_last_seen_yyyymmdd = 0;
static uint32_t g_kbo_foreign_waiver_last_close_event_end = 0;
static uint32_t g_kbo_foreign_waiver_start_event_date = 0;
static uint32_t g_kbo_foreign_waiver_close_event_end_date = 0;
static uint32_t g_kbo_foreign_waiver_last_result_announcement = 0;
static volatile LONG g_kbo_foreign_priority_pending_lock = 0;
static uint32_t g_kbo_foreign_priority_pending_yyyymmdd = 0;
static char g_kbo_foreign_priority_pending_title[96] = {0};
static char g_kbo_foreign_priority_pending_source[48] = {0};
static LONG g_kbo_foreign_waiver_rights_lock = 0;
static LONG g_kbo_foreign_waiver_decision_lock = 0;

#define KBO_FOREIGN_WAIVER_RETENTION_YEARS 5
#define KBO_FOREIGN_WAIVER_RIGHTS_MAX 1024
#define KBO_FOREIGN_WAIVER_AI_AUTO_TEAM_SLOT_MAX 512

typedef struct KboForeignWaiverRetention {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t retained_on_yyyymmdd;
    uint32_t expires_on_yyyymmdd;
} KboForeignWaiverRetention;

typedef struct KboForeignWaiverAiTargetCandidate {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t current_team_id;
    int score;
    int forced;
} KboForeignWaiverAiTargetCandidate;

static KboForeignWaiverRetention g_kbo_foreign_waiver_rights[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
static int g_kbo_foreign_waiver_rights_count = 0;

static void kbo_queue_foreign_priority_league_event(
    uint32_t event_yyyymmdd,
    const char* title,
    const char* source)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_priority_pending_lock, 1, 0) != 0) {
        Sleep(0);
    }

    g_kbo_foreign_priority_pending_yyyymmdd = event_yyyymmdd;
    snprintf(
        g_kbo_foreign_priority_pending_title,
        sizeof(g_kbo_foreign_priority_pending_title),
        "%s",
        title != NULL ? title : "");
    snprintf(
        g_kbo_foreign_priority_pending_source,
        sizeof(g_kbo_foreign_priority_pending_source),
        "%s",
        source != NULL ? source : "foreign_priority_negotiation");

    InterlockedExchange(&g_kbo_foreign_priority_pending_lock, 0);
    append_logf(
        "foreign priority negotiation: league event queued source=%s title=%s date=%u",
        g_kbo_foreign_priority_pending_source,
        g_kbo_foreign_priority_pending_title,
        event_yyyymmdd);
}

static void kbo_flush_pending_foreign_priority_events(const char* source)
{
    uint32_t event_yyyymmdd = 0;
    char title[96] = {0};
    char queued_source[48] = {0};

    while (InterlockedCompareExchange(&g_kbo_foreign_priority_pending_lock, 1, 0) != 0) {
        Sleep(0);
    }

    event_yyyymmdd = g_kbo_foreign_priority_pending_yyyymmdd;
    if (event_yyyymmdd != 0u) {
        snprintf(title, sizeof(title), "%s", g_kbo_foreign_priority_pending_title);
        snprintf(queued_source, sizeof(queued_source), "%s", g_kbo_foreign_priority_pending_source);
        g_kbo_foreign_priority_pending_yyyymmdd = 0;
        g_kbo_foreign_priority_pending_title[0] = '\0';
        g_kbo_foreign_priority_pending_source[0] = '\0';
    }

    InterlockedExchange(&g_kbo_foreign_priority_pending_lock, 0);

    if (event_yyyymmdd == 0u || title[0] == '\0') {
        return;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        append_logf(
            "foreign priority negotiation: pending league event dropped source=%s title=%s date=%u reason=invalid_date",
            queued_source,
            title,
            event_yyyymmdd);
        return;
    }

    int created = create_kbo_league_event(
        year,
        month,
        day,
        kbo_get_foreign_waiver_league_id(),
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        0,
        queued_source[0] != '\0' ? queued_source : source);

    append_logf(
        "foreign priority negotiation: pending league event flushed source=%s flush_source=%s title=%s date=%u created=%d",
        queued_source,
        source != NULL ? source : "",
        title,
        event_yyyymmdd,
        created);
}

static int kbo_emit_foreign_priority_league_event(
    uint32_t event_yyyymmdd,
    const char* title,
    const char* source)
{
    if (event_yyyymmdd == 0u || title == NULL || title[0] == '\0') {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    if (create_kbo_league_event(
            year,
            month,
            day,
            kbo_get_foreign_waiver_league_id(),
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            title,
            0,
            source != NULL ? source : "foreign_priority_negotiation")) {
        append_logf(
            "foreign priority negotiation: league event created immediately source=%s title=%s date=%u",
            source != NULL ? source : "",
            title,
            event_yyyymmdd);
        return 1;
    }

    kbo_queue_foreign_priority_league_event(event_yyyymmdd, title, source);
    return 1;
}

static uint32_t kbo_detect_offseason_starts_event(uint32_t today_yyyymmdd, uint32_t league_id)
{
    static LONG debug_log_count = 0;
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t today_key = today_yyyymmdd;
    LONG debug_slot = InterlockedIncrement(&debug_log_count);
    int debug_remaining = debug_slot <= 6 ? 12 : 0;
    uint32_t latest_offseason_start = 0u;

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (league_id != 0u && *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_league_id = *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET);
        if (event_year < 1980u || event_year > 2200u || event_month < 1u || event_month > 12u || event_day < 1u || event_day > 31u) {
            continue;
        }

        uint32_t event_key = event_year * 10000u + event_month * 100u + event_day;
        if (event_key > today_key) {
            continue;
        }

        char name[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (debug_remaining > 0
                && event_month >= 9u
                && event_month <= 12u) {
            append_logf(
                "foreign waiver auto: event probe idx=%d date=%04u-%02u-%02u league=%u type=%u over=%u name=%s event=%p",
                i,
                event_year,
                event_month,
                event_day,
                event_league_id,
                (uint32_t)(*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET)),
                (uint32_t)(*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET)),
                name,
                (void*)event_ptr);
            debug_remaining--;
        }
        if (_stricmp(name, "Offseason starts") == 0) {
            if (event_key > latest_offseason_start) {
                latest_offseason_start = event_key;
            }
        }
    }

    if (latest_offseason_start != 0u) {
        append_logf(
            "foreign waiver auto: detected latest Offseason starts date=%u (today=%u)",
            latest_offseason_start,
            today_yyyymmdd);
    }

    return latest_offseason_start;
}

static int kbo_days_in_month(uint32_t year, uint32_t month)
{
    static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1u || month > 12u) {
        return 0;
    }
    if (month == 2u && kbo_is_leap_year(year)) {
        return 29;
    }
    return days[month];
}

static uint32_t kbo_add_one_month_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1980u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }
    month++;
    if (month > 12u) {
        month = 1u;
        year++;
    }
    int dim = kbo_days_in_month(year, month);
    if (dim <= 0) {
        return 0;
    }
    if (day > (uint32_t)dim) {
        day = (uint32_t)dim;
    }
    return year * 10000u + month * 100u + day;
}

static uint32_t kbo_add_days_yyyymmdd(uint32_t yyyymmdd, uint32_t add_days)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1980u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    while (add_days > 0u) {
        int dim = kbo_days_in_month(year, month);
        if (dim <= 0) {
            return 0;
        }
        day++;
        if (day > (uint32_t)dim) {
            day = 1u;
            month++;
            if (month > 12u) {
                month = 1u;
                year++;
            }
        }
        add_days--;
    }
    return year * 10000u + month * 100u + day;
}

static int kbo_write_foreign_waiver_window(uint32_t start_yyyymmdd, uint32_t end_yyyymmdd, const char* reason)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_event_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign priority negotiation: failed to write window file error=%lu", GetLastError());
        return 0;
    }

    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%08u,%08u\r\n", start_yyyymmdd, end_yyyymmdd);
    DWORD written = 0;
    int ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    CloseHandle(file);

    append_logf(
        "foreign priority negotiation: event window persisted start=%u end=%u reason=%s ok=%d path=%s",
        start_yyyymmdd,
        end_yyyymmdd,
        reason == NULL ? "" : reason,
        ok,
        path);
    return ok;
}

static int kbo_open_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial, const char* reason)
{
    uint32_t end_yyyymmdd = kbo_add_days_yyyymmdd(today_yyyymmdd, 20u);
    if (end_yyyymmdd == 0u) {
        return 0;
    }

    g_kbo_foreign_waiver_window_start_serial = today_serial;
    g_kbo_foreign_waiver_window_end_serial = today_serial + 20u;
    kbo_write_foreign_waiver_window(today_yyyymmdd, end_yyyymmdd, reason);
    g_kbo_foreign_waiver_start_event_date = today_yyyymmdd;
    append_logf(
        "foreign priority negotiation: season-end event opened start=%u end=%u serial=%u~%u reason=%s",
        today_yyyymmdd,
        end_yyyymmdd,
        g_kbo_foreign_waiver_window_start_serial,
        g_kbo_foreign_waiver_window_end_serial,
        reason == NULL ? "" : reason);
    return 1;
}

static int kbo_advance_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial)
{
    append_logf(
        "foreign waiver auto: evaluate season-end check today=%u serial=%u prev_marker=%u",
        today_yyyymmdd, today_serial,
        g_kbo_foreign_waiver_last_seen_yyyymmdd);

    if (today_yyyymmdd == 0 || today_serial == 0) {
        append_log_line("foreign waiver auto: season-end check skipped - invalid date");
        return 0;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        append_log_line("foreign waiver auto: missing configured league id; skip season-end evaluation");
        return 0;
    }

    uint32_t season_end_yyyymmdd = kbo_detect_offseason_starts_event(today_yyyymmdd, configured_league_id);
    if (season_end_yyyymmdd == 0u) {
        append_logf("foreign waiver auto: no Offseason starts event found today=%u", today_yyyymmdd);
        g_kbo_foreign_waiver_last_seen_yyyymmdd = 0u;
        return 0;
    }
    if (today_yyyymmdd < season_end_yyyymmdd) {
        append_logf(
            "foreign waiver auto: waiting for Offseason starts marker season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        return 0;
    }
    if (g_kbo_foreign_waiver_last_seen_yyyymmdd == season_end_yyyymmdd) {
        append_logf(
            "foreign waiver auto: already opened for marker=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        return 0;
    }

    g_kbo_foreign_waiver_last_seen_yyyymmdd = season_end_yyyymmdd;
    append_logf(
        "foreign waiver auto: season-end marker confirmed season_end=%u today=%u",
        season_end_yyyymmdd,
        today_yyyymmdd);
    return kbo_open_foreign_waiver_window(
        today_yyyymmdd,
        today_serial,
        "offseason_starts_event");
}

static int kbo_parse_yyyymmdd(const char* date_text, uint32_t* out_date)
{
    if (date_text == NULL || out_date == NULL) {
        return 0;
    }

    char ymd[9] = {0};
    size_t len = strlen(date_text);
    if (len >= 8
            && date_text[0] >= '0' && date_text[0] <= '9'
            && date_text[1] >= '0' && date_text[1] <= '9'
            && date_text[2] >= '0' && date_text[2] <= '9'
            && date_text[3] >= '0' && date_text[3] <= '9'
            && date_text[4] >= '0' && date_text[4] <= '9'
            && date_text[5] >= '0' && date_text[5] <= '9'
            && date_text[6] >= '0' && date_text[6] <= '9'
            && date_text[7] >= '0' && date_text[7] <= '9') {
        memcpy(ymd, date_text, 8);
    } else if (len >= 10 && date_text[4] == '-' && date_text[7] == '-') {
        ymd[0] = date_text[0]; ymd[1] = date_text[1]; ymd[2] = date_text[2]; ymd[3] = date_text[3];
        ymd[4] = date_text[5]; ymd[5] = date_text[6];
        ymd[6] = date_text[8]; ymd[7] = date_text[9];
    } else {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        if (ymd[i] < '0' || ymd[i] > '9') {
            return 0;
        }
    }

    *out_date = (uint32_t)(
        (ymd[0] - '0') * 10000000u +
        (ymd[1] - '0') * 1000000u +
        (ymd[2] - '0') * 100000u +
        (ymd[3] - '0') * 10000u +
        (ymd[4] - '0') * 1000u +
        (ymd[5] - '0') * 100u +
        (ymd[6] - '0') * 10u +
        (ymd[7] - '0'));
    return 1;
}

static int kbo_get_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date == NULL) {
        return 0;
    }

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    *out_date = year * 10000u + month * 100u + day;
    return 1;
}

static int kbo_get_foreign_waiver_rights_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_waiver_rights.csv", local_app_data);
    return 1;
}

static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_waiver_decisions.csv", local_app_data);
    return 1;
}

static int get_kbo_foreign_waiver_announcement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_waiver_announcements.txt", local_app_data);
    return 1;
}

static int kbo_foreign_waiver_announcement_recorded(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char raw[4096] = {0};
    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, raw, sizeof(raw) - 1u, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        const char* cursor = raw;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n' || *cursor == ',') {
                cursor++;
            }
            char* tail = NULL;
            unsigned long raw_date = strtoul(cursor, &tail, 10);
            if (raw_date == event_yyyymmdd) {
                found = 1;
                break;
            }
            if (tail == cursor) {
                break;
            }
            cursor = tail;
        }
    }

    CloseHandle(file);
    return found;
}

static void kbo_record_foreign_waiver_announcement(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char line[32] = {0};
    int len = snprintf(line, sizeof(line), "%u\r\n", event_yyyymmdd);
    DWORD written = 0;
    if (len > 0) {
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
}

static void kbo_record_foreign_waiver_announcement_body(uint32_t event_yyyymmdd, const char* source, const char* body)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign reserve rights: announcement body record failed path=%s err=%lu", path, GetLastError());
        return;
    }

    char line[1400] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u|%s|%s\r\n",
        event_yyyymmdd,
        source != NULL ? source : "",
        body != NULL ? body : "");
    DWORD written = 0;
    if (len > 0) {
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
}

static int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date)
{
    return kbo_get_current_yyyymmdd(out_date);
}

static uint32_t kbo_add_years_yyyymmdd(uint32_t yyyymmdd, uint32_t add_years)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }
    year += add_years;
    if (year > 2200u) {
        year = 2200u;
    }
    int dim = kbo_days_in_month(year, month);
    if (dim <= 0) {
        return 0u;
    }
    if (day > (uint32_t)dim) {
        day = (uint32_t)dim;
    }
    return year * 10000u + month * 100u + day;
}

static int kbo_is_foreign_waiver_right_active(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL || rec->player_id == 0u || rec->team_id == 0u || rec->expires_on_yyyymmdd == 0u || today == 0u) {
        return 0;
    }
    return today <= rec->expires_on_yyyymmdd;
}

static int kbo_load_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD file_size = GetFileSize(file, NULL);
    if (file_size == INVALID_FILE_SIZE || file_size == 0) {
        CloseHandle(file);
        return 0;
    }
    char raw[65536] = {0};
    DWORD read = 0;
    if (file_size >= sizeof(raw)) {
        file_size = sizeof(raw) - 1u;
    }
    ReadFile(file, raw, file_size, &read, NULL);
    CloseHandle(file);
    if (read == 0u) {
        return 0;
    }
    raw[read] = '\0';

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    g_kbo_foreign_waiver_rights_count = 0;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    char* cursor = raw;
    int deduped = 0;
    while (*cursor != '\0') {
        char* next = strchr(cursor, '\n');
        if (next == NULL) {
            next = cursor + strlen(cursor);
        }
        size_t len = (size_t)(next - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }
        if (len > 8) {
            char line[128] = {0};
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);
            const char* p = line;
            if (line[0] != '#' && line[0] != ';') {
                uint32_t player_id = 0;
                uint32_t team_id = 0;
                uint32_t league_id = 0;
                uint32_t retained_on = 0;
                uint32_t expires_on = 0;
                if (parse_u32_from_csv_field((const char**)&p, &player_id)
                    && parse_u32_from_csv_field((const char**)&p, &team_id)
                    && parse_u32_from_csv_field((const char**)&p, &league_id)
                    && parse_u32_from_csv_field((const char**)&p, &retained_on)
                    && parse_u32_from_csv_field((const char**)&p, &expires_on)) {
                    if (player_id != 0u && team_id != 0u && league_id != 0u) {
                        while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
                            Sleep(0);
                        }
                        int existing_index = -1;
                        for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
                            if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
                                existing_index = i;
                                break;
                            }
                        }
                        if (existing_index >= 0) {
                            KboForeignWaiverRetention* existing = &g_kbo_foreign_waiver_rights[existing_index];
                            if (retained_on >= existing->retained_on_yyyymmdd) {
                                *existing = (KboForeignWaiverRetention){
                                    player_id,
                                    team_id,
                                    league_id,
                                    retained_on,
                                    expires_on
                                };
                            }
                            deduped++;
                        } else if (g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
                            g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
                                player_id,
                                team_id,
                                league_id,
                                retained_on,
                                expires_on
                            };
                        }
                        InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
                    }
                }
            }
        }
        if (*next == '\0') {
            break;
        }
        cursor = next + 1;
    }
    append_logf("foreign reserve rights: loaded=%d deduped=%d", g_kbo_foreign_waiver_rights_count, deduped);
    if (deduped > 0) {
        kbo_persist_foreign_waiver_rights();
    }
    return 1;
}

static int kbo_persist_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
        return 0;
    }
    char header[64] = "player_id,team_id,league_id,retained_on,expires_on\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == 0u || rec->team_id == 0u) {
            continue;
        }
        char line[128] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u\r\n",
            rec->player_id,
            rec->team_id,
            rec->league_id,
            rec->retained_on_yyyymmdd,
            rec->expires_on_yyyymmdd);
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    append_logf("foreign reserve rights: persisted=%d", g_kbo_foreign_waiver_rights_count);
    return 1;
}

static int kbo_read_foreign_waiver_window(uint32_t* out_start, uint32_t* out_end)
{
    if (out_start == NULL || out_end == NULL) {
        return 0;
    }
    *out_start = 0;
    *out_end = 0;

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_event_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char raw[128] = {0};
    DWORD read = 0;
    if (!ReadFile(file, raw, sizeof(raw) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    if (read == 0) {
        return 0;
    }

    char* first = raw;
    char* second = NULL;
    while (*first == ' ' || *first == '\r' || *first == '\n' || *first == '\t') {
        first++;
    }
    if (*first == '#') {
        return 0;
    }
    char* sep = strchr(first, ',');
    if (sep == NULL) {
        return 0;
    }
    *sep = '\0';
    second = sep + 1;
    while (*second == ' ' || *second == '\r' || *second == '\n' || *second == '\t') {
        second++;
    }

    if (!kbo_parse_yyyymmdd(first, out_start)) {
        return 0;
    }
    if (!kbo_parse_yyyymmdd(second, out_end)) {
        return 0;
    }
    return 1;
}

static int kbo_is_foreign_waiver_negotiation_window_open(void)
{
    if (!kbo_fix_enabled() || !kbo_foreign_waiver_ai_enabled()) {
        return 0;
    }

    uint32_t today = 0;
    if (!kbo_get_current_yyyymmdd(&today)) {
        return 0;
    }

    uint32_t today_serial = kbo_date_serial(today / 10000u, (today / 100u) % 100u, today % 100u);
    kbo_advance_foreign_waiver_window(today, today_serial);

    uint32_t start = 0;
    uint32_t end = 0;
    if (!kbo_read_foreign_waiver_window(&start, &end)) {
        int open = (g_kbo_foreign_waiver_window_end_serial != 0
                && today_serial >= g_kbo_foreign_waiver_window_start_serial
                && today_serial < g_kbo_foreign_waiver_window_end_serial);
        if (!open && g_kbo_foreign_waiver_window_end_serial != 0
                && today_serial > g_kbo_foreign_waiver_window_end_serial) {
            if (g_kbo_foreign_waiver_last_close_event_end != g_kbo_foreign_waiver_window_end_serial) {
                g_kbo_foreign_waiver_last_close_event_end = g_kbo_foreign_waiver_window_end_serial;
                append_logf(
                    "foreign priority negotiation: event closed today=%u end_serial=%u source=auto detector",
                    today,
                    g_kbo_foreign_waiver_window_end_serial);
            }
        }
        return open;
    }

    if (today >= start && today < end) {
        return 1;
    }
    return 0;
}

static int kbo_build_foreign_waiver_result_body(char* out, size_t out_size, uint32_t today_yyyymmdd)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    int retained = 0;
    int skipped = 0;
    int ai_retained = 0;
    int ai_skipped = 0;
    int user_retained = 0;
    int user_skipped = 0;
    int active_rights = 0;

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (kbo_is_foreign_waiver_right_active(&g_kbo_foreign_waiver_rights[i], today_yyyymmdd)) {
            active_rights++;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    char decision_path[MAX_PATH] = {0};
    if (get_kbo_foreign_waiver_decisions_path(decision_path, sizeof(decision_path))) {
        HANDLE file = CreateFileA(decision_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD size = GetFileSize(file, NULL);
            if (size != INVALID_FILE_SIZE && size > 0 && size < 262144u) {
                char* input = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
                DWORD read = 0;
                if (input != NULL && ReadFile(file, input, size, &read, NULL) && read > 0) {
                    input[read] = '\0';
                    char* cursor = input;
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

                        const char* p = line;
                        uint32_t decision_date = 0u;
                        uint32_t window_start = 0u;
                        uint32_t window_end = 0u;
                        if (line[0] >= '0' && line[0] <= '9'
                                && parse_u32_from_csv_field(&p, &decision_date)
                                && parse_u32_from_csv_field(&p, &window_start)
                                && parse_u32_from_csv_field(&p, &window_end)
                                && window_end == today_yyyymmdd) {
                            char source_name[16] = {0};
                            char action_name[16] = {0};
                            while (*p == ',' || *p == ' ' || *p == '\t') { p++; }
                            size_t si = 0u;
                            while (*p != '\0' && *p != ',' && si + 1u < sizeof(source_name)) {
                                source_name[si++] = *p++;
                            }
                            source_name[si] = '\0';
                            if (*p == ',') { p++; }
                            size_t ai = 0u;
                            while (*p != '\0' && *p != ',' && ai + 1u < sizeof(action_name)) {
                                action_name[ai++] = *p++;
                            }
                            action_name[ai] = '\0';

                            if (_stricmp(action_name, "RETAIN") == 0) {
                                retained++;
                                if (_stricmp(source_name, "ai") == 0) { ai_retained++; }
                                else if (_stricmp(source_name, "user") == 0) { user_retained++; }
                            } else if (_stricmp(action_name, "SKIP") == 0) {
                                skipped++;
                                if (_stricmp(source_name, "ai") == 0) { ai_skipped++; }
                                else if (_stricmp(source_name, "user") == 0) { user_skipped++; }
                            }
                        }

                        if (*next == '\0') {
                            break;
                        }
                        cursor = next + 1;
                    }
                }
                if (input != NULL) {
                    HeapFree(GetProcessHeap(), 0, input);
                }
            }
            CloseHandle(file);
        }
    }

    if (retained == 0 && skipped == 0) {
        char rights_path[MAX_PATH] = {0};
        if (kbo_get_foreign_waiver_rights_path(rights_path, sizeof(rights_path))) {
            HANDLE file = CreateFileA(rights_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(file, NULL);
                if (size != INVALID_FILE_SIZE && size > 0u && size < 65536u) {
                    char input[65536] = {0};
                    DWORD read = 0;
                    if (ReadFile(file, input, size, &read, NULL) && read > 0u) {
                        input[read] = '\0';
                        const char* cursor = input;
                        while (*cursor != '\0') {
                            uint32_t player_id = 0u;
                            uint32_t team_id = 0u;
                            uint32_t league_id = 0u;
                            uint32_t retained_on = 0u;
                            uint32_t expires_on = 0u;
                            const char* p = cursor;
                            if (cursor[0] >= '0' && cursor[0] <= '9'
                                    && parse_u32_from_csv_field(&p, &player_id)
                                    && parse_u32_from_csv_field(&p, &team_id)
                                    && parse_u32_from_csv_field(&p, &league_id)
                                    && parse_u32_from_csv_field(&p, &retained_on)
                                    && parse_u32_from_csv_field(&p, &expires_on)
                                    && retained_on <= today_yyyymmdd
                                    && expires_on >= today_yyyymmdd) {
                                retained++;
                            }
                            while (*cursor != '\0' && *cursor != '\n') {
                                cursor++;
                            }
                            if (*cursor == '\n') {
                                cursor++;
                            }
                        }
                    }
                }
                CloseHandle(file);
            }
        }
    }

    snprintf(
        out,
        out_size,
        "The 20-day KBO foreign player reserve-rights decision period has ended. Decisions recorded: retain=%d, skip=%d. AI clubs: retain=%d, skip=%d. User club: retain=%d, skip=%d. Active stored reserve-right records: %d. Full five-year exclusive negotiation enforcement will be added in a later phase.",
        retained,
        skipped,
        ai_retained,
        ai_skipped,
        user_retained,
        user_skipped,
        active_rights);
    return 1;
}

static int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source)
{
    if (event_yyyymmdd == 0u || g_kbo_foreign_waiver_last_result_announcement == event_yyyymmdd
            || kbo_foreign_waiver_announcement_recorded(event_yyyymmdd)) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char body[1024] = {0};
    kbo_load_foreign_waiver_rights();
    kbo_build_foreign_waiver_result_body(body, sizeof(body), event_yyyymmdd);

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        "[KBO] Foreign Player Reserve Rights Results",
        body);
    kbo_record_foreign_waiver_announcement(event_yyyymmdd);
    kbo_record_foreign_waiver_announcement_body(event_yyyymmdd, source, body);
    g_kbo_foreign_waiver_last_result_announcement = event_yyyymmdd;
    append_logf(
        "foreign reserve rights: result announcement source=%s date=%u created=%d mode=file_recorded_native_news_disabled body=%s",
        source != NULL ? source : "",
        event_yyyymmdd,
        created,
        body);
    return 1;
}

static void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd)
{
    if (today_yyyymmdd == 0u) {
        return;
    }
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    int w = 0;
    int removed = 0;
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (!kbo_is_foreign_waiver_right_active(&g_kbo_foreign_waiver_rights[i], today_yyyymmdd)) {
            removed++;
            continue;
        }
        if (w != i) {
            g_kbo_foreign_waiver_rights[w] = g_kbo_foreign_waiver_rights[i];
        }
        w++;
    }
    for (int i = w; i < g_kbo_foreign_waiver_rights_count; i++) {
        memset(&g_kbo_foreign_waiver_rights[i], 0, sizeof(KboForeignWaiverRetention));
    }
    g_kbo_foreign_waiver_rights_count = w;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    if (removed > 0) {
        append_logf("foreign reserve rights: expired=%d today=%u", removed, today_yyyymmdd);
        kbo_persist_foreign_waiver_rights();
    }
}

static int kbo_set_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t league_id,
    uint32_t retained_on,
    uint32_t expires_on)
{
    if (team_id == 0u || player_id == 0u || league_id == 0u || retained_on == 0u || expires_on == 0u) {
        return 0;
    }

    int upserted = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
            if (g_kbo_foreign_waiver_rights[i].team_id != team_id) {
                append_logf(
                    "foreign reserve rights: reassigned exclusive holder player=%u old_team=%u new_team=%u",
                    player_id,
                    g_kbo_foreign_waiver_rights[i].team_id,
                    team_id);
            }
            g_kbo_foreign_waiver_rights[i].team_id = team_id;
            g_kbo_foreign_waiver_rights[i].league_id = league_id;
            g_kbo_foreign_waiver_rights[i].retained_on_yyyymmdd = retained_on;
            g_kbo_foreign_waiver_rights[i].expires_on_yyyymmdd = expires_on;
            upserted = 1;
            break;
        }
    }
    if (!upserted && g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
        g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
            player_id,
            team_id,
            league_id,
            retained_on,
            expires_on
        };
        upserted = 1;
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (upserted) {
        kbo_persist_foreign_waiver_rights();
    }
    return upserted;
}

static int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd)
{
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }
    int result = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            result = 1;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    return result;
}

static int kbo_find_active_foreign_waiver_holder(uint32_t player_id, uint32_t today_yyyymmdd, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0;
    }
    if (player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    int found = 0;
    uint32_t holder_team_id = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            holder_team_id = rec->team_id;
            found = holder_team_id != 0u;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (found && out_team_id != NULL) {
        *out_team_id = holder_team_id;
    }
    return found;
}

static void kbo_record_recent_foreign_offer_block(uint32_t player_id, uint32_t requester_team_id, uint32_t holder_team_id, uint32_t today);
static int kbo_recent_foreign_offer_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id, uint32_t* out_holder_team_id);

static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability)
{
    (void)year_hint;
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return original_signability;
    }
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return original_signability;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        return original_signability;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return original_signability;
    }

    uint32_t today = 0;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_signability;
    }

    static LONG load_attempted = 0;
    if (InterlockedCompareExchange(&load_attempted, 1, 0) == 0) {
        kbo_load_foreign_waiver_rights();
    }
    kbo_prune_expired_foreign_waiver_rights(today);

    uint32_t holder_team_id = 0;
    if (!kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
        return original_signability;
    }

    if (requesting_team_id <= 0) {
        static LONG generic_block_log_count = 0;
        LONG slot = InterlockedIncrement(&generic_block_log_count);
        if (slot <= 200) {
            append_logf(
                "foreign reserve signability: blocked generic request player=%u holder_team=%u original=%d forced=0 today=%u",
                player_id,
                holder_team_id,
                original_signability,
                today);
        }
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    uint32_t team_id = (uint32_t)requesting_team_id;
    if (team_id == holder_team_id) {
        static LONG allow_log_count = 0;
        LONG slot = InterlockedIncrement(&allow_log_count);
        if (slot <= 40) {
            append_logf(
                "foreign reserve signability: holder allowed player=%u team=%u original=%d today=%u",
                player_id,
                team_id,
                original_signability,
                today);
        }
        return original_signability;
    }

    static LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 200) {
        append_logf(
            "foreign reserve signability: blocked player=%u requester_team=%u holder_team=%u original=%d forced=0 today=%u",
            player_id,
            team_id,
            holder_team_id,
            original_signability,
            today);
    }
    kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
    return 0; /* OOTP signability enum: 0 = Impossible. */
}

typedef int (__fastcall *OotpPlayerTeamSignabilityFn)(void* player, int32_t team_id, uint16_t year_hint);
typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);

static volatile LONG g_kbo_foreign_offer_block_player_id = 0;
static volatile LONG g_kbo_foreign_offer_block_requester_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_holder_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_date = 0;
static volatile LONG64 g_kbo_foreign_offer_block_tick = 0;

static void kbo_record_recent_foreign_offer_block(uint32_t player_id, uint32_t requester_team_id, uint32_t holder_team_id, uint32_t today)
{
    InterlockedExchange(&g_kbo_foreign_offer_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_holder_team_id, (LONG)holder_team_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_foreign_offer_block_tick, (LONG64)GetTickCount64());
}

static int kbo_recent_foreign_offer_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id, uint32_t* out_holder_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_foreign_offer_block_date, 0, 0);
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_foreign_offer_block_tick, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (cached_tick <= 0 || now < (ULONGLONG)cached_tick || now - (ULONGLONG)cached_tick > 120000ull) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0);
    LONG holder = InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0);
    if (requester <= 0 || holder <= 0 || requester == holder) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = (uint32_t)holder;
    }
    return 1;
}

static uint32_t kbo_resolve_submit_offer_screen_player_id(uintptr_t screen_ptr, uint32_t today, uint32_t* out_offset)
{
    if (out_offset != NULL) {
        *out_offset = 0xffffffffu;
    }
    if (screen_ptr == 0 || !memory_range_readable((void*)screen_ptr, 0x400)) {
        return 0;
    }

    uint32_t direct_player_id = *(uint32_t*)(screen_ptr + 0xb8u);
    uint32_t holder_team_id = 0;
    if (direct_player_id != 0u && kbo_find_active_foreign_waiver_holder(direct_player_id, today, &holder_team_id)) {
        if (out_offset != NULL) {
            *out_offset = 0xb8u;
        }
        return direct_player_id;
    }

    for (uint32_t offset = 0x20u; offset < 0x400u; offset += sizeof(uint32_t)) {
        uint32_t candidate_id = *(uint32_t*)(screen_ptr + offset);
        if (candidate_id == 0u || candidate_id > 10000000u) {
            continue;
        }
        if (kbo_find_active_foreign_waiver_holder(candidate_id, today, &holder_team_id)) {
            if (out_offset != NULL) {
                *out_offset = offset;
            }
            return candidate_id;
        }
    }
    return direct_player_id;
}

__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uint16_t year_hint,
    uintptr_t original_func_ptr)
{
    OotpPlayerTeamSignabilityFn original_func = (OotpPlayerTeamSignabilityFn)original_func_ptr;
    int original_signability = 0;
    if (original_func != NULL) {
        original_signability = original_func((void*)player_ptr, team_id, year_hint);
    }
    return kbo_enforce_foreign_waiver_signability(player_ptr, team_id, year_hint, original_signability);
}

__declspec(noinline) void ootp_kbo_fa_submit_offer_probe_wrapper(
    uintptr_t screen_ptr,
    uintptr_t original_func_ptr)
{
    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0xc0)) {
        uint32_t today = 0;
        uint32_t holder_team_id = 0;
        uint32_t requester_team_id = 0;
        uint32_t player_offset = 0xffffffffu;
        uint32_t player_id = 0;
        if (kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            player_id = kbo_resolve_submit_offer_screen_player_id(screen_ptr, today, &player_offset);
        }
        static LONG entry_log_count = 0;
        LONG entry_slot = InterlockedIncrement(&entry_log_count);
        if (entry_slot <= 80) {
            append_logf(
                "foreign reserve submit-offer entry: screen=%p player=%u offset=0x%x today=%u cached_player=%ld cached_team=%ld cached_holder=%ld",
                (void*)screen_ptr,
                player_id,
                player_offset,
                today,
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0));
        }
        if (player_id != 0u
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            static LONG probe_log_count = 0;
            LONG slot = InterlockedIncrement(&probe_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign reserve submit-offer probe: screen=%p player=%u holder_team=%u today=%u",
                    (void*)screen_ptr,
                    player_id,
                    holder_team_id,
                    today);
            }
            if (kbo_recent_foreign_offer_block_matches(player_id, today, &requester_team_id, &holder_team_id)) {
                static LONG submit_block_log_count = 0;
                LONG block_slot = InterlockedIncrement(&submit_block_log_count);
                if (block_slot <= 200) {
                    append_logf(
                        "foreign reserve submit-offer blocked: screen=%p player=%u requester_team=%u holder_team=%u today=%u",
                        (void*)screen_ptr,
                        player_id,
                        requester_team_id,
                        holder_team_id,
                        today);
                }
                return;
            }
        }
    }

    OotpFaSubmitOfferProbeFn original_func = (OotpFaSubmitOfferProbeFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)screen_ptr);
    }
}

__declspec(noinline) int32_t ootp_kbo_ai_fa_status_candidate_insert_wrapper(
    uintptr_t frame_ptr,
    uintptr_t player_ptr,
    int32_t insert_index,
    uintptr_t candidate_array)
{
    if (frame_ptr == 0 || player_ptr == 0 || insert_index < 0) {
        return insert_index;
    }
    if (!memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return insert_index;
    }

    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0;
    uint32_t holder_team_id = 0;
    uint32_t requester_team_id = 0;
    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + 0x140u);
    if (team_ptr != 0 && memory_range_readable((void*)(team_ptr + 0x4450u), sizeof(uint32_t))) {
        requester_team_id = *(uint32_t*)(team_ptr + 0x4450u);
    }

    if (player_id != 0u
        && requester_team_id != 0u
        && kbo_get_foreign_waiver_current_yyyymmdd(&today)
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u
        && holder_team_id != requester_team_id) {
        static LONG block_log_count = 0;
        LONG slot = InterlockedIncrement(&block_log_count);
        if (slot <= 300) {
            append_logf(
                "foreign reserve AI FA status candidate blocked player=%u requester_team=%u holder_team=%u index=%d today=%u",
                player_id,
                requester_team_id,
                holder_team_id,
                insert_index,
                today);
        }
        return insert_index;
    }

    if (candidate_array == 0) {
        return insert_index;
    }
    if (!memory_range_readable((void*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))), sizeof(uintptr_t))) {
        return insert_index;
    }
    *(uintptr_t*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))) = player_ptr;
    *(int32_t*)(frame_ptr - 0x2c) = insert_index + 1;
    return insert_index + 1;
}

static int kbo_append_foreign_waiver_cmd_line(const char* line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_cmd_path(path, sizeof(path))) {
        return 0;
    }
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_decision_lock, 1, 0) != 0) {
        Sleep(0);
    }
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
        return 0;
    }
    DWORD wrote = 0;
    char out[128] = {0};
    int len = snprintf(out, sizeof(out), "%s\r\n", line);
    WriteFile(file, out, (DWORD)len, &wrote, NULL);
    CloseHandle(file);
    InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
    return wrote == (DWORD)len;
}

static int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        append_logf("foreign waiver decision: blocked by window state team=%u player=%u action=%s", team_id, player_id, retain ? "RETAIN" : "SKIP");
        return 0;
    }
    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%s,%u,%u", retain ? "RETAIN" : "SKIP", team_id, player_id);
    if (len <= 0) {
        return 0;
    }
    return kbo_append_foreign_waiver_cmd_line(line);
}

static int kbo_format_ymd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size < 11u) {
        return 0;
    }
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year == 0u || month == 0u || day == 0u
            || month > 12u || day > 31u || year < 1980u) {
        return 0;
    }
    return (snprintf(out, out_size, "%04u-%02u-%02u", year, month, day) > 0) ? 1 : 0;
}

static int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    if (!kbo_foreign_waiver_ai_enabled()) {
        snprintf(out, out_size, "Foreign waiver negotiation: disabled (run launcher once after this update to create enable_foreign_waiver_ai.txt)");
        return 1;
    }

    if (!kbo_fix_enabled()) {
        snprintf(out, out_size, "Foreign waiver negotiation: disabled (KBO fix off)");
        return 1;
    }

    uint32_t today = 0;
    if (!kbo_get_current_yyyymmdd(&today)) {
        snprintf(out, out_size, "Foreign waiver negotiation: unavailable (failed to read game date)");
        return 1;
    }
    int window_open = kbo_is_foreign_waiver_negotiation_window_open();
    uint32_t file_start = 0;
    uint32_t file_end = 0;
    const char* source = "auto detector";
    if (kbo_read_foreign_waiver_window(&file_start, &file_end)) {
        source = "override file";
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (!kbo_format_ymd(file_start, start_text, sizeof(start_text))
            || !kbo_format_ymd(file_end, end_text, sizeof(end_text))) {
        snprintf(start_text, sizeof(start_text), "-");
        snprintf(end_text, sizeof(end_text), "-");
    }

    const char* status = window_open ? "OPEN" : "CLOSED";
    snprintf(
        out,
        out_size,
        "Foreign waiver priority window [league=%u] (%s): %s [Window=%s ~ %s]",
        configured_league_id,
        source,
        status,
        start_text,
        end_text);
    return 1;
}

static uint32_t read_u32_leading_number_from_file(const char* filename)
{
    if (filename == NULL) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\%s", local_app_data, filename);

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buf[128] = {0};
    DWORD read = 0;
    if (!ReadFile(file, buf, sizeof(buf) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    const char* cursor = buf;
    while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }
    if (*cursor == '\0') {
        return 0;
    }

    char* tail = NULL;
    unsigned long long raw = strtoull(cursor, &tail, 10);
    if (raw == 0ULL || raw > UINT32_MAX) {
        return 0;
    }
    return (uint32_t)raw;
}

static uint32_t kbo_get_foreign_waiver_auto_target_team_id(void)
{
    return read_u32_leading_number_from_file("foreign_waiver_ai_targets.txt");
}

static int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_waiver_commands.txt", local_app_data);
    return 1;
}

static int get_kbo_foreign_waiver_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_waiver_candidates.csv", local_app_data);
    return 1;
}

static int append_foreign_waiver_candidate_csv_header(HANDLE file)
{
    const char* header = "date,source,player_id,current_team_id,active_team_id,original_team_id,current_league_id,priority_window_open,priority_team_id,priority_eligible,dfa_flag,restricted,secondary_restricted,loan_active,injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static int kbo_is_forced_foreign_candidate_id(uint32_t player_id)
{
    if (player_id == 0) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\foreign_player_ids.txt", local_app_data);

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buf[4096] = {0};
    DWORD read = 0;
    if (!ReadFile(file, buf, sizeof(buf) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    const char* cursor = buf;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ',') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char* tail = NULL;
        unsigned long long raw = strtoull(cursor, &tail, 10);
        if (raw == 0ULL && cursor == tail) {
            break;
        }

        if (raw == (unsigned long long)player_id) {
            return 1;
        }

        cursor = tail;
        while (*cursor != '\0' && *cursor != ',' && *cursor != '\n' && *cursor != '\r') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        while (*cursor == ',' || *cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
    }

    return 0;
}

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

static uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + 0x5cu, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + 0x5cu);
}

static uint32_t kbo_get_foreign_waiver_decision_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (active_team_id != 0u) {
        return active_team_id;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (current_team_id != 0u) {
        return current_team_id;
    }

    return kbo_get_player_original_team_id(player);
}

static int kbo_original_club_priority_window_allows(uint8_t* player, uint32_t team_id, const char* action_name)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 1;
    }

    uint32_t original_team_id = kbo_get_player_original_team_id(player);
    if (original_team_id == 0 || team_id == original_team_id) {
        return 1;
    }

    uint32_t player_id = 0;
    if (player != NULL && memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    }
    append_logf(
        "foreign priority negotiation: blocked action=%s team=%u player=%u original_team=%u",
        action_name == NULL ? "" : action_name,
        team_id,
        player_id,
        original_team_id);
    return 0;
}

static int kbo_retain_foreign_player_rights(
    uint8_t* player,
    uint8_t* retaining_team,
    uint32_t fallback_league_id,
    uint32_t player_id,
    uint32_t team_id)
{
    if (player == NULL || retaining_team == NULL || player_id == 0 || team_id == 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(retaining_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_numeric_id = *(uint32_t*)(retaining_team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (team_numeric_id != team_id) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(retaining_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0) {
        team_league_id = fallback_league_id;
    }
    if (team_league_id == 0) {
        return 0;
    }

    uint32_t today_yyyymmdd = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today_yyyymmdd)) {
        return 0;
    }
    uint32_t expires_yyyymmdd = kbo_add_years_yyyymmdd(today_yyyymmdd, KBO_FOREIGN_WAIVER_RETENTION_YEARS);
    if (expires_yyyymmdd == 0u) {
        return 0;
    }

    if (!kbo_set_foreign_waiver_right(team_id, player_id, team_league_id, today_yyyymmdd, expires_yyyymmdd)) {
        append_logf("foreign reserve rights: failed to store right team=%u player=%u", team_id, player_id);
        return 0;
    }

    if (!kbo_add_player_id_to_team_fixed_array(retaining_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
        append_logf("foreign reserve rights: restricted array full team=%u player=%u", team_id, player_id);
        return 0;
    }

    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1;
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = team_league_id;

    append_logf("foreign reserve rights: retained team=%u player=%u league=%u from=%u until=%u",
               team_id, player_id, team_league_id, today_yyyymmdd, expires_yyyymmdd);
    return 1;
}

static int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value)
{
    if (cursor == NULL || *cursor == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == ',') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    unsigned long long value = 0;
    while (*p >= '0' && *p <= '9') {
        value = (value * 10ULL) + (unsigned long long)(*p - '0');
        if (value > UINT32_MAX) {
            return 0;
        }
        p++;
    }

    *out_value = (uint32_t)value;
    *cursor = p;
    return 1;
}

static int kbo_current_foreign_waiver_window_dates(uint32_t* out_start, uint32_t* out_end)
{
    if (out_start != NULL) { *out_start = 0u; }
    if (out_end != NULL) { *out_end = 0u; }

    uint32_t start = 0u;
    uint32_t end = 0u;
    if (kbo_read_foreign_waiver_window(&start, &end)) {
        if (out_start != NULL) { *out_start = start; }
        if (out_end != NULL) { *out_end = end; }
        return start != 0u && end != 0u;
    }

    if (g_kbo_foreign_waiver_start_event_date != 0u && g_kbo_foreign_waiver_close_event_end_date != 0u) {
        if (out_start != NULL) { *out_start = g_kbo_foreign_waiver_start_event_date; }
        if (out_end != NULL) { *out_end = g_kbo_foreign_waiver_close_event_end_date; }
        return 1;
    }

    return 0;
}

static int kbo_append_foreign_waiver_decision_record(
    const char* source,
    const char* action,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed)
{
    if (source == NULL || source[0] == '\0' || action == NULL || action[0] == '\0'
            || team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_decisions_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);

    uint32_t window_start = 0u;
    uint32_t window_end = 0u;
    kbo_current_foreign_waiver_window_dates(&window_start, &window_end);

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_decision_lock, 1, 0) != 0) {
        Sleep(0);
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
        return 0;
    }

    DWORD written = 0;
    if (needs_header) {
        const char* header = "decision_date,window_start,window_end,source,action,team_id,player_id,value_score,forced,executed\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char line[256] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,%s,%s,%u,%u,%d,%d,%d\r\n",
        today,
        window_start,
        window_end,
        source,
        action,
        team_id,
        player_id,
        score,
        forced ? 1 : 0,
        executed ? 1 : 0);
    int ok = 0;
    if (len > 0 && len < (int)sizeof(line)) {
        written = 0;
        ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    }

    CloseHandle(file);
    InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
    return ok;
}

static int kbo_foreign_waiver_decision_exists(uint32_t window_end, uint32_t team_id, uint32_t player_id)
{
    if (window_end == 0u || team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_decisions_path(path, sizeof(path))) {
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
    int found = 0;
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

            const char* p = line;
            uint32_t decision_date = 0u;
            uint32_t row_start = 0u;
            uint32_t row_end = 0u;
            uint32_t row_team = 0u;
            uint32_t row_player = 0u;
            if (line[0] >= '0' && line[0] <= '9'
                    && parse_u32_from_csv_field(&p, &decision_date)
                    && parse_u32_from_csv_field(&p, &row_start)
                    && parse_u32_from_csv_field(&p, &row_end)) {
                for (int comma = 0; comma < 3 && *p != '\0'; comma++) {
                    while (*p != '\0' && *p != ',') { p++; }
                    if (*p == ',') { p++; }
                }
                if (parse_u32_from_csv_field(&p, &row_team)
                        && parse_u32_from_csv_field(&p, &row_player)
                        && row_end == window_end
                        && row_team == team_id
                        && row_player == player_id) {
                    found = 1;
                    break;
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
    return found;
}

static int kbo_execute_foreign_waiver_claim(const char* line, int line_no)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    if (line[0] == '#' || line[0] == ';') {
        return 0;
    }

    char raw[512];
    size_t raw_len = 0;
    for (; raw_len < sizeof(raw) - 1 && line[raw_len] != '\0' && line[raw_len] != '\r' && line[raw_len] != '\n'; raw_len++) {
        raw[raw_len] = line[raw_len];
    }
    raw[raw_len] = '\0';

    const char* p = raw;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    char action_name[16] = {0};
    size_t idx = 0;
    while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t' && idx + 1 < sizeof(action_name)) {
        action_name[idx++] = *p++;
    }
    action_name[idx] = '\0';

    int retain_rights =
        strcmp(action_name, "RETAIN") == 0 || strcmp(action_name, "retain") == 0
        || strcmp(action_name, "RIGHTS") == 0 || strcmp(action_name, "rights") == 0
        || strcmp(action_name, "RESERVE") == 0 || strcmp(action_name, "reserve") == 0;
    int skip_rights =
        strcmp(action_name, "SKIP") == 0 || strcmp(action_name, "skip") == 0;

    if (strcmp(action_name, "CLAIM") != 0 && strcmp(action_name, "claim") != 0
            && strcmp(action_name, "MOVE") != 0 && strcmp(action_name, "move") != 0
            && !retain_rights && !skip_rights) {
        return 0;
    }

    if (*p == ',') {
        p++;
    } else if (*p != '\0') {
        while (*p != '\0' && *p != ',') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }

    uint32_t team_id = 0;
    uint32_t player_id = 0;
    if (!parse_u32_from_csv_field(&p, &team_id) || !parse_u32_from_csv_field(&p, &player_id)) {
        append_logf("foreign waiver command line %d malformed: %s", line_no, raw);
        return 0;
    }

    if (team_id == 0 || player_id == 0) {
        append_logf("foreign waiver command line %d ignored (zero id): %s", line_no, raw);
        return 0;
    }

    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (destination_team == NULL) {
        append_logf("foreign waiver command line %d failed: team=%u not found", line_no, team_id);
        return 0;
    }

    uint32_t player_current_team = 0;
    uint32_t player_current_league = 0;
    uint8_t* player = kbo_find_player_by_id(player_id, &player_current_team, &player_current_league);
    if (player == NULL) {
        append_logf("foreign waiver command line %d failed: player=%u not found", line_no, player_id);
        return 0;
    }

    if (!kbo_original_club_priority_window_allows(player, team_id, action_name)) {
        return 0;
    }

    if (retain_rights) {
        uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
        if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
            append_logf("foreign waiver command line %d failed: action=%s team=%u player=%u",
                       line_no, action_name, team_id, player_id);
            return 0;
        }

        append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
        return 1;
    }

    if (skip_rights) {
        append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "SKIP", team_id, player_id, 0, 0, 1);
        return 1;
    }

    if (player_current_team == team_id) {
        append_logf("foreign waiver command line %d no-op: player=%u already on team=%u", line_no, player_id, team_id);
        return 1;
    }

    uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
    if (fallback_league == 0) {
        return 0;
    }

    if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
        append_logf("foreign waiver command line %d failed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        return 0;
    }
    append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
               line_no, action_name, team_id, player_id);
    kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
    return 1;
}

static int kbo_apply_ai_foreign_waiver_rules(
    uint32_t player_id,
    uint32_t player_current_team_id,
    int value_score,
    int forced,
    uint32_t target_team_id)
{
    (void)player_current_team_id;

    if (target_team_id == 0 || player_id == 0) {
        return 0;
    }

    if (!forced && value_score < kbo_get_foreign_waiver_value_threshold()) {
        return 0;
    }

    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(target_team_id, 1);
    if (destination_team == NULL) {
        append_logf("foreign waiver auto: target team not found target_team=%u", target_team_id);
        return 0;
    }

    uint32_t player_current_league_id = 0;
    uint8_t* player = kbo_find_player_by_id(player_id, &player_current_team_id, &player_current_league_id);
    if (player == NULL) {
        return 0;
    }

    if (player_current_team_id == target_team_id) {
        return 1;
    }

    uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t fallback_league = player_current_league_id != 0 ? player_current_league_id : destination_league;
    if (fallback_league == 0) {
        return 0;
    }

    if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, target_team_id)) {
        append_logf("foreign waiver auto: retain failed player=%u -> team=%u value=%d forced=%d",
            player_id, target_team_id, value_score, forced);
        return 0;
    }

    append_logf(
        "foreign waiver auto: retained player=%u -> team=%u value=%d forced=%d",
        player_id, target_team_id, value_score, forced);
    kbo_append_foreign_waiver_decision_record("ai", "RETAIN", target_team_id, player_id, value_score, forced, 1);
    return 1;
}

static void run_foreign_waiver_ai_core_once(void)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }
    {
        static LONG rights_loaded = 0;
        if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
    }

    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        append_logf(
            "foreign waiver auto: window check -> CLOSED (auto window: %u~%u)",
            g_kbo_foreign_waiver_window_start_serial,
            g_kbo_foreign_waiver_window_end_serial);
        append_log_line("foreign waiver auto: negotiation window is closed");
        return;
    }
    append_logf(
        "foreign waiver auto: window check -> OPEN (auto window: %u~%u)",
        g_kbo_foreign_waiver_window_start_serial,
        g_kbo_foreign_waiver_window_end_serial);

    uint32_t target_team_id = kbo_get_foreign_waiver_auto_target_team_id();
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return;
    }
    uint32_t window_start = 0u;
    uint32_t window_end = 0u;
    kbo_current_foreign_waiver_window_dates(&window_start, &window_end);
    kbo_prune_expired_foreign_waiver_rights(today);

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    int considered = 0;
    int retained = 0;
    int skipped = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        if (target_team_id != 0u && decision_team_id != target_team_id) {
            continue;
        }
        if (decision_team_id == 0u) {
            continue;
        }

        uint8_t* decision_team = find_kbo_team_by_numeric_id_any_league(decision_team_id, 1);
        if (decision_team == NULL) {
            continue;
        }
        uint32_t team_league_id = *(uint32_t*)(decision_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (configured_league_id != 0u && team_league_id != configured_league_id) {
            continue;
        }

        if (kbo_has_active_foreign_waiver_right(decision_team_id, player_id, today)) {
            continue;
        }
        if (kbo_foreign_waiver_decision_exists(window_end, decision_team_id, player_id)) {
            continue;
        }

        int forced = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        int retain = forced || score >= kbo_get_foreign_waiver_value_threshold();
        considered++;

        if (retain) {
            if (kbo_apply_ai_foreign_waiver_rules(player_id, current_team_id, score, forced, decision_team_id)) {
                retained++;
            } else {
                skipped++;
                append_logf(
                    "foreign waiver auto: SKIP player=%u team=%u value=%d forced=%d reason=retain_failed",
                    player_id, decision_team_id, score, forced);
                kbo_append_foreign_waiver_decision_record("ai", "SKIP", decision_team_id, player_id, score, forced, 0);
            }
        } else {
            skipped++;
            append_logf(
                "foreign waiver auto: SKIP player=%u team=%u value=%d forced=%d",
                player_id, decision_team_id, score, forced);
            kbo_append_foreign_waiver_decision_record("ai", "SKIP", decision_team_id, player_id, score, forced, 1);
        }
    }

    append_logf(
        "foreign waiver auto: all-player decisions considered=%d retained=%d skipped=%d target_team=%u",
        considered,
        retained,
        skipped,
        target_team_id);
}

static int kbo_resolve_foreign_waiver_top_candidate_for_team(
    uint32_t team_id,
    uint32_t* out_player_id,
    uint32_t* out_current_team_id)
{
    if (team_id == 0u || out_player_id == NULL || out_current_team_id == NULL) {
        return 0;
    }
    *out_player_id = 0u;
    *out_current_team_id = 0u;
    {
        static LONG rights_loaded = 0;
        if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return 0;
    }
    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int best_score = -1;
    uint32_t best_player_id = 0u;
    uint32_t best_current_team_id = 0u;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (player_id == 0u || decision_team_id != team_id
                || (current_league_id != 0u && current_league_id != configured_league_id)
                || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        int score = kbo_foreign_waiver_value_score(player);
        if (kbo_has_active_foreign_waiver_right(team_id, player_id, today)) {
            continue;
        }

        if (score > best_score) {
            best_score = score;
            best_player_id = player_id;
            best_current_team_id = current_team_id;
        }
    }

    if (best_player_id == 0u) {
        return 0;
    }
    *out_player_id = best_player_id;
    *out_current_team_id = best_current_team_id;
    return 1;
}

static void process_foreign_waiver_commands(void)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_cmd_path(path, sizeof(path))) {
        append_log_line("foreign waiver command: unable to resolve command path");
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD file_size_low = GetFileSize(file, NULL);
    if (file_size_low == INVALID_FILE_SIZE || file_size_low == 0) {
        CloseHandle(file);
        return;
    }
    if (file_size_low >= 30000) {
        CloseHandle(file);
        append_log_line("foreign waiver command: command file too large; skip to avoid blocking");
        return;
    }

    char input[30000] = {0};
    DWORD read = 0;
    if (!ReadFile(file, input, file_size_low, &read, NULL) || read == 0) {
        CloseHandle(file);
        return;
    }
    CloseHandle(file);
    input[read] = '\0';

    char remaining[30000] = {0};
    char* keep = remaining;
    DWORD remain_len = 0;
    DWORD used_commands = 0;
    DWORD executed_commands = 0;

    const char* cursor = input;
    int line_no = 1;
    while (cursor < input + read) {
        const char* line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = input + read;
        }

        size_t len = (size_t)(line_end - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }

        char line[512] = {0};
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }
        memcpy(line, cursor, len);
        line[len] = '\0';

        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }

        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            if (remain_len + len + 1 < sizeof(remaining)) {
                size_t add_len = strlen(line);
                memcpy(keep + remain_len, line, add_len);
                keep[remain_len + add_len] = '\r';
                keep[remain_len + add_len + 1] = '\n';
                remain_len += (DWORD)(add_len + 2);
            }
        } else {
            used_commands++;
            if (!kbo_execute_foreign_waiver_claim(trimmed, line_no)) {
                if (remain_len + len + 2 < sizeof(remaining)) {
                    size_t add_len = strlen(trimmed);
                    memcpy(keep + remain_len, trimmed, add_len);
                    keep[remain_len + add_len] = '\r';
                    keep[remain_len + add_len + 1] = '\n';
                    remain_len += (DWORD)(add_len + 2);
                }
            } else {
                executed_commands++;
            }
        }

        if (*line_end == '\0') {
            break;
        }
        cursor = line_end + 1;
        line_no++;
    }

    if (used_commands > 0) {
        file = CreateFileA(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            if (remain_len > 0) {
                DWORD written = 0;
                WriteFile(file, remaining, remain_len, &written, NULL);
            }
            CloseHandle(file);
        }
        append_logf("foreign waiver command: processed=%lu executed=%lu keep_len=%lu", used_commands, executed_commands, remain_len);
        return;
    }
}

static int is_csv_empty(HANDLE file)
{
    DWORD high = 0;
    uint32_t low = GetFileSize(file, &high);
    if (high != 0) {
        return 0;
    }
    return low == 0;
}

static void write_foreign_waiver_candidates(const char* source)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_log_line("foreign waiver scanner: no player vector");
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_csv_path(path, sizeof(path))) {
        append_log_line("foreign waiver scanner: unable to resolve output path");
        return;
    }

    {
        char dir[MAX_PATH] = {0};
        size_t len = strlen(path);
        if (len > 0 && len < sizeof(dir)) {
            strcpy_s(dir, sizeof(dir), path);
            char* slash = strrchr(dir, '\\');
            if (slash != NULL) {
                *slash = '\0';
                CreateDirectoryA(dir, NULL);
            }
        }
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign waiver scanner: failed to open %s", path);
        return;
    }

    if (is_csv_empty(file)) {
        append_foreign_waiver_candidate_csv_header(file);
    }
    SetFilePointer(file, 0, NULL, FILE_END);

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    int priority_window_open = kbo_is_foreign_waiver_negotiation_window_open();

    int scanned = 0;
    int written_candidates = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        uint8_t loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

        int forced_foreign = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        if (!forced_foreign && score <= 0) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t original_team_id = kbo_get_player_original_team_id(player);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        uint32_t priority_team_id = priority_window_open ? decision_team_id : 0;
        int priority_eligible = priority_window_open && decision_team_id != 0u;

        char line[320] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            date,
            (source == NULL ? "" : source),
            player_id,
            current_team_id,
            active_team_id,
            original_team_id,
            current_league_id,
            (uint32_t)priority_window_open,
            priority_team_id,
            (uint32_t)priority_eligible,
            (uint32_t)dfa,
            (uint32_t)restricted,
            (uint32_t)secondary,
            (uint32_t)loan_active,
            (uint32_t)inj_active,
            score);

        DWORD written = 0;
        WriteFile(file, line, (DWORD)len, &written, NULL);
        written_candidates++;
    }

    CloseHandle(file);

    append_logf("foreign waiver scanner: scanned=%d candidates=%d file=%s", scanned, written_candidates, path);
}

static DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t tick = 0;
    for (;;) {
        Sleep(5000);
        tick++;
        process_foreign_waiver_commands();
        run_foreign_waiver_ai_core_once();
        if ((tick % 6u) == 0u) {
            write_foreign_waiver_candidates("foreign_waiver_scanner");
        }
    }
    return 0;
}

static void start_kbo_foreign_waiver_scanner_thread(void)
{
    static LONG started = 0;
    if (!kbo_foreign_waiver_ai_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_waiver_scanner_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("foreign waiver scanner thread started");
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("foreign waiver scanner thread failed to start");
    }
}
