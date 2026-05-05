#include <stdint.h>

/* ---- Foreign waiver intelligence scaffold ---- */

#define KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT 3u

static uint32_t read_u32_leading_number_from_file(const char* filename);
static int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size);
static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size);
static int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size);
static int kbo_load_foreign_waiver_rights(void);
static int kbo_persist_foreign_waiver_rights(void);
static void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd);
static int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value);
static int kbo_player_is_foreign_for_kbo_rights(uint8_t* player);
static int kbo_player_is_asian_quota_candidate(uint8_t* player);
static void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count);
static uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count);
static void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers);
static const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
static int kbo_foreign_injury_replacement_signing_exception_available(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_effective_count,
    uint32_t* out_effective_limit);
static int kbo_custom_foreign_policy_enabled(void);
static int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
static int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id);
static void kbo_record_recent_custom_foreign_policy_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today);
static int kbo_recent_custom_foreign_policy_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id);
static int32_t kbo_custom_foreign_policy_neutralized_count(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count);
static uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type);

static int kbo_foreign_waiver_ai_enabled(void)
{
    return read_kbo_localappdata_flag_file("enable_foreign_waiver_ai.txt");
}

static int kbo_custom_foreign_policy_enabled(void)
{
    return kbo_fix_enabled() && !read_kbo_localappdata_flag_file("disable_kbo_custom_foreign_policy.txt");
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
    return kbo_get_save_scoped_data_file("foreign_waiver_negotiation_window.txt", out, out_size);
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
#define KBO_FOREIGN_ROSTER_AUDIT_MAX 8192

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

typedef struct KboForeignRosterAuditState {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t current_league_id;
    uint32_t loan_team_id;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t injury_active;
    int score;
    uint32_t seen_generation;
} KboForeignRosterAuditState;

static KboForeignWaiverRetention g_kbo_foreign_waiver_rights[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
static int g_kbo_foreign_waiver_rights_count = 0;
static KboForeignRosterAuditState g_kbo_foreign_roster_audit[KBO_FOREIGN_ROSTER_AUDIT_MAX] = {{0}};
static int g_kbo_foreign_roster_audit_count = 0;
static uint32_t g_kbo_foreign_roster_audit_generation = 0u;
static char g_kbo_foreign_roster_audit_save_path[MAX_PATH] = {0};

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
    static uint32_t last_checked_yyyymmdd = 0u;
    static uint32_t last_checked_result = 0u;
    if (today_yyyymmdd != 0u
            && today_yyyymmdd == last_checked_yyyymmdd
            && last_checked_result != 1u) {
        return 0;
    }
    last_checked_yyyymmdd = today_yyyymmdd;
    last_checked_result = 0u;

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
    int opened = kbo_open_foreign_waiver_window(
        today_yyyymmdd,
        today_serial,
        "offseason_starts_event");
    last_checked_result = opened ? 1u : 2u;
    return opened;
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
    return kbo_get_save_scoped_data_file("foreign_waiver_rights.csv", out, out_size);
}

static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_decisions.csv", out, out_size);
}

static int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\asian_quota_nation_ids.txt", local_app_data);
    return 1;
}

static int get_kbo_foreign_waiver_announcement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_announcements.txt", out, out_size);
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
        "The 20-day KBO foreign player reserve-rights decision period has ended. Decisions recorded: retain=%d, skip=%d. AI clubs: retain=%d, skip=%d. User club: retain=%d, skip=%d. Active stored reserve-right records: %d. Retained players are protected by five-year exclusive negotiation rights.",
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
    static volatile LONG rights_loaded = 0;
    if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
        kbo_load_foreign_waiver_rights();
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

    static volatile LONG rights_loaded = 0;
    if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
        kbo_load_foreign_waiver_rights();
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

static int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd)
{
    if (player == NULL || player_id == 0u || holder_team_id == 0u || today_yyyymmdd == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t league_id = 0u;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id
                && rec->team_id == holder_team_id
                && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            league_id = rec->league_id;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    uint8_t* holder_team = find_kbo_team_by_numeric_id_any_league(holder_team_id, 1);
    if (holder_team == NULL || !memory_range_readable(holder_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    if (league_id == 0u) {
        league_id = *(uint32_t*)(holder_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    if (league_id == 0u) {
        return 0;
    }

    int changed = 0;
    if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != holder_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = holder_team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (kbo_add_player_id_to_team_fixed_array(holder_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
        changed = 1;
    }

    if (changed) {
        static LONG sync_log_count = 0;
        LONG slot = InterlockedIncrement(&sync_log_count);
        if (slot <= 200) {
            append_logf(
                "foreign reserve rights: synced active right to memory player=%u holder_team=%u league=%u today=%u",
                player_id,
                holder_team_id,
                league_id,
                today_yyyymmdd);
        }
    }
    return 1;
}

static void kbo_record_recent_foreign_offer_block(uint32_t player_id, uint32_t requester_team_id, uint32_t holder_team_id, uint32_t today);
static int kbo_recent_foreign_offer_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id, uint32_t* out_holder_team_id);
static void kbo_log_asian_quota_offer_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    uint8_t original_result,
    int32_t flag);
static void kbo_log_asian_quota_signability_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    int original_signability,
    uintptr_t caller_rva);

static void kbo_log_foreign_signability_block_callsite(
    uintptr_t caller_rva,
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today)
{
    enum { KBO_SIGNABILITY_CALLSITE_LOG_SLOTS = 64 };
    static volatile LONG logged_count = 0;
    static uintptr_t logged_callers[KBO_SIGNABILITY_CALLSITE_LOG_SLOTS] = {0};

    if (caller_rva == 0) {
        return;
    }
    for (int i = 0; i < KBO_SIGNABILITY_CALLSITE_LOG_SLOTS; i++) {
        if (logged_callers[i] == caller_rva) {
            return;
        }
    }

    LONG slot = InterlockedIncrement(&logged_count) - 1;
    if (slot < 0 || slot >= KBO_SIGNABILITY_CALLSITE_LOG_SLOTS) {
        return;
    }
    logged_callers[slot] = caller_rva;
    append_logf(
        "foreign reserve signability: new block callsite caller_rva=0x%llx player=%u requester_team=%u holder_team=%u original=%d today=%u",
        (unsigned long long)caller_rva,
        player_id,
        requester_team_id,
        holder_team_id,
        original_signability,
        today);
}

static int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    static uint32_t cached_sang_id = 0u;
    static uint32_t cached_kpb_id = 0u;
    if (cached_sang_id == 0u) {
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        if (sang != NULL && memory_range_readable(sang + OOTP27_KBO_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            cached_sang_id = *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET);
        }
    }
    if (cached_kpb_id == 0u) {
        uint8_t* kpb = find_kbo_team_by_csv_id_any_league("KPB", 0);
        if (kpb != NULL && memory_range_readable(kpb + OOTP27_KBO_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            cached_kpb_id = *(uint32_t*)(kpb + OOTP27_KBO_TEAM_ID_OFFSET);
        }
    }

    return team_id == cached_sang_id || (cached_kpb_id != 0u && team_id == cached_kpb_id);
}

static int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || requesting_team_id <= 0) {
        return 0;
    }

    uint32_t requester_team_id = (uint32_t)requesting_team_id;
    if (kbo_team_id_is_military_service_team(requester_team_id)) {
        uint32_t player_id = 0u;
        if (player_ptr != 0
                && memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
            player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
        }
        if (out_player_id != NULL) {
            *out_player_id = player_id;
        }
        static volatile LONG military_fast_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_fast_block_log_count);
        if (slot <= 120) {
            append_logf(
                "FA fast-block source=%s reason=military_service_team player=%u requester_team=%u",
                context != NULL ? context : "",
                player_id,
                requester_team_id);
        }
        return 1;
    }

    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }
    if (player_id == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return 0;
    }

    if (kbo_foreign_waiver_ai_enabled()) {
        static LONG load_attempted = 0;
        if (InterlockedCompareExchange(&load_attempted, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
        kbo_prune_expired_foreign_waiver_rights(today);

        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u
                && holder_team_id != requester_team_id) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
            kbo_record_recent_foreign_offer_block(player_id, requester_team_id, holder_team_id, today);

            static volatile LONG rights_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&rights_fast_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "FA fast-block source=%s reason=foreign_reserve player=%u requester_team=%u holder_team=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    holder_team_id,
                    today);
            }
            return 1;
        }
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            static volatile LONG custom_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_fast_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "FA fast-block source=%s reason=custom_foreign_policy player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            }
            return 1;
        }
    }

    return 0;
}

static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva)
{
    (void)year_hint;
    if (!kbo_fix_enabled()) {
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

    if (requesting_team_id > 0 && kbo_team_id_is_military_service_team((uint32_t)requesting_team_id)) {
        static volatile LONG military_team_signability_log_count = 0;
        LONG slot = InterlockedIncrement(&military_team_signability_log_count);
        if (slot <= 200) {
            append_logf(
                "military service team FA signability blocked player=%u requester_team=%d original=%d caller_rva=0x%llx",
                player_id,
                requesting_team_id,
                original_signability,
                (unsigned long long)caller_rva);
        }
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    kbo_log_asian_quota_signability_probe(player, player_id, requesting_team_id, original_signability, caller_rva);

    uint32_t today = 0;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_signability;
    }

    uint32_t holder_team_id = 0;
    if (kbo_foreign_waiver_ai_enabled()) {
        static LONG load_attempted = 0;
        if (InterlockedCompareExchange(&load_attempted, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
        kbo_prune_expired_foreign_waiver_rights(today);

        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

            if (requesting_team_id <= 0) {
                static LONG generic_block_log_count = 0;
                LONG slot = InterlockedIncrement(&generic_block_log_count);
                if (slot <= 200) {
                    append_logf(
                        "foreign reserve signability: blocked generic request player=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                        player_id,
                        holder_team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                return 0; /* OOTP signability enum: 0 = Impossible. */
            }

            uint32_t team_id = (uint32_t)requesting_team_id;
            if (team_id == holder_team_id) {
                static LONG allow_log_count = 0;
                LONG slot = InterlockedIncrement(&allow_log_count);
                if (slot <= 40) {
                    append_logf(
                        "foreign reserve signability: holder allowed player=%u team=%u original=%d today=%u caller_rva=0x%llx",
                        player_id,
                        team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                return original_signability;
            }

            static LONG block_log_count = 0;
            LONG slot = InterlockedIncrement(&block_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign reserve signability: blocked player=%u requester_team=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                    player_id,
                    team_id,
                    holder_team_id,
                    original_signability,
                    today,
                    (unsigned long long)caller_rva);
            }
            kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
            kbo_log_foreign_signability_block_callsite(caller_rva, player_id, team_id, holder_team_id, original_signability, today);
            return 0; /* OOTP signability enum: 0 = Impossible. */
        }
    }

    if (requesting_team_id > 0
            && kbo_custom_foreign_policy_enabled()
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)requesting_team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(
            player,
            (uint32_t)requesting_team_id);
        int adjusted = allowed ? original_signability : 0;
        if (allowed && original_signability == 0 && override_original_block) {
            adjusted = 1;
        }

        static volatile LONG custom_policy_signability_log_count = 0;
        LONG slot = InterlockedIncrement(&custom_policy_signability_log_count);
        if (slot <= 200) {
            append_logf(
                "custom foreign policy signability player=%u requester_team=%d original=%d adjusted=%d allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u caller_rva=0x%llx",
                player_id,
                requesting_team_id,
                original_signability,
                adjusted,
                allowed,
                override_original_block,
                effective_before,
                effective_after,
                effective_limit,
                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                injured_player_id,
                today,
                (unsigned long long)caller_rva);
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)requesting_team_id, today);
        }
        return adjusted;
    }

    if (requesting_team_id > 0) {
        uint8_t injury_slot_type = 0u;
        uint32_t injured_player_id = 0u;
        uint32_t effective_count = 0u;
        uint32_t effective_limit = 0u;
        if (kbo_foreign_injury_replacement_signing_exception_available(
                (uint32_t)requesting_team_id,
                player,
                &injury_slot_type,
                &injured_player_id,
                &effective_count,
                &effective_limit)) {
            static LONG injury_signability_log_count = 0;
            LONG slot = InterlockedIncrement(&injury_signability_log_count);
            int adjusted = original_signability != 0 ? original_signability : 1;
            if (slot <= 120) {
                append_logf(
                    "foreign injury replacement signability allowed player=%u requester_team=%d injured=%u slot=%s original=%d adjusted=%d effective=%u limit=%u today=%u caller_rva=0x%llx",
                    player_id,
                    requesting_team_id,
                    injured_player_id,
                    kbo_foreign_injury_slot_label(injury_slot_type),
                    original_signability,
                    adjusted,
                    effective_count,
                    effective_limit,
                    today,
                    (unsigned long long)caller_rva);
            }
            return adjusted;
        }
    }

    return original_signability;
}

typedef int (__fastcall *OotpPlayerTeamSignabilityFn)(void* player, int32_t team_id, uint16_t year_hint);
typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);

static volatile LONG g_kbo_foreign_offer_block_player_id = 0;
static volatile LONG g_kbo_foreign_offer_block_requester_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_holder_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_date = 0;
static volatile LONG64 g_kbo_foreign_offer_block_tick = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_player_id = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_requester_team_id = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_date = 0;
static volatile LONG64 g_kbo_custom_foreign_policy_block_tick = 0;

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

static void kbo_record_recent_custom_foreign_policy_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    if (player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    InterlockedExchange(&g_kbo_custom_foreign_policy_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_custom_foreign_policy_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_custom_foreign_policy_block_tick, (LONG64)GetTickCount64());
}

static int kbo_recent_custom_foreign_policy_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_date, 0, 0);
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_custom_foreign_policy_block_tick, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (cached_tick <= 0 || now < (ULONGLONG)cached_tick || now - (ULONGLONG)cached_tick > 120000ull) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, 0, 0);
    if (requester <= 0) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
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
    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "signability", NULL)) {
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    OotpPlayerTeamSignabilityFn original_func = (OotpPlayerTeamSignabilityFn)original_func_ptr;
    int original_signability = 0;
    if (original_func != NULL) {
        original_signability = original_func((void*)player_ptr, team_id, year_hint);
    }
    uintptr_t caller_rva = 0;
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    if (exe != NULL && caller > (uintptr_t)exe) {
        caller_rva = caller - (uintptr_t)exe;
    }
    return kbo_enforce_foreign_waiver_signability(player_ptr, team_id, year_hint, original_signability, caller_rva);
}

__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uintptr_t original_func_ptr)
{
    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "offer_eligibility", NULL)) {
        return 0;
    }

    typedef uint8_t (__fastcall *OriginalOfferEligibilityFn)(void*, int32_t, int32_t);
    OriginalOfferEligibilityFn original_func = (OriginalOfferEligibilityFn)original_func_ptr;

    uint8_t original_result = 0;
    if (original_func != NULL) {
        original_result = original_func((void*)player_ptr, team_id, flag);
    }

    if (player_ptr == 0 || team_id <= 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return original_result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    kbo_log_asian_quota_offer_probe(player, player_id, team_id, original_result, flag);

    if (kbo_team_id_is_military_service_team((uint32_t)team_id)) {
        static volatile LONG military_team_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&military_team_offer_log_count);
        if (slot <= 120) {
            append_logf(
                "military service team FA offer eligibility blocked player=%u requester_team=%d original=%u flag=%d",
                player_id,
                team_id,
                (uint32_t)original_result,
                flag);
        }
        return 0;
    }

    uint32_t today = 0u;
    uint32_t holder_team_id = 0u;
    if (player_id == 0u || !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_result;
    }

    if (kbo_foreign_waiver_ai_enabled()
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            && holder_team_id != 0u) {
        if (holder_team_id == (uint32_t)team_id) {
            return original_result;
        }

        kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 80) {
            append_logf(
                "foreign reserve offer eligibility blocked player=%u requester_team=%d holder_team=%u original=%u flag=%d today=%u",
                player_id,
                team_id,
                holder_team_id,
                (uint32_t)original_result,
                flag,
                today);
        }

        kbo_record_recent_foreign_offer_block(player_id, (uint32_t)team_id, holder_team_id, today);
        return 0;
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(player, (uint32_t)team_id);
        uint8_t adjusted = allowed ? original_result : 0u;
        if (allowed && original_result == 0u && override_original_block) {
            adjusted = 1u;
        }

        static volatile LONG custom_policy_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&custom_policy_offer_log_count);
        if (slot <= 200) {
            append_logf(
                "custom foreign policy offer player=%u requester_team=%d original=%u adjusted=%u allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u flag=%d today=%u",
                player_id,
                team_id,
                (uint32_t)original_result,
                (uint32_t)adjusted,
                allowed,
                override_original_block,
                effective_before,
                effective_after,
                effective_limit,
                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                injured_player_id,
                flag,
                today);
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)team_id, today);
        }
        return adjusted;
    }

    uint8_t injury_slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_limit = 0u;
    if (kbo_foreign_injury_replacement_signing_exception_available(
            (uint32_t)team_id,
            player,
            &injury_slot_type,
            &injured_player_id,
            &effective_count,
            &effective_limit)) {
        static volatile LONG injury_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&injury_offer_log_count);
        uint8_t adjusted = original_result != 0u ? original_result : 1u;
        if (slot <= 120) {
            append_logf(
                "foreign injury replacement offer eligibility allowed player=%u requester_team=%d injured=%u slot=%s original=%u adjusted=%u effective=%u limit=%u flag=%d today=%u",
                player_id,
                team_id,
                injured_player_id,
                kbo_foreign_injury_slot_label(injury_slot_type),
                (uint32_t)original_result,
                (uint32_t)adjusted,
                effective_count,
                effective_limit,
                flag,
                today);
        }
        return adjusted;
    }

    return original_result;
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
                "foreign reserve submit-offer entry: screen=%p player=%u offset=0x%x today=%u cached_player=%ld cached_team=%ld cached_holder=%ld custom_cached_player=%ld custom_cached_team=%ld",
                (void*)screen_ptr,
                player_id,
                player_offset,
                today,
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, 0, 0));
        }
        if (player_id != 0u
                && kbo_recent_custom_foreign_policy_block_matches(player_id, today, &requester_team_id)) {
            static LONG custom_submit_block_log_count = 0;
            LONG block_slot = InterlockedIncrement(&custom_submit_block_log_count);
            if (block_slot <= 200) {
                append_logf(
                    "custom foreign policy submit-offer blocked: screen=%p player=%u requester_team=%u today=%u",
                    (void*)screen_ptr,
                    player_id,
                    requester_team_id,
                    today);
            }
            return;
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
            int recent_block = kbo_recent_foreign_offer_block_matches(player_id, today, &requester_team_id, &holder_team_id);
            if (!recent_block || requester_team_id != holder_team_id) {
                static LONG submit_block_log_count = 0;
                LONG block_slot = InterlockedIncrement(&submit_block_log_count);
                if (block_slot <= 200) {
                    append_logf(
                        "foreign reserve submit-offer blocked: screen=%p player=%u requester_team=%u holder_team=%u today=%u reason=%s",
                        (void*)screen_ptr,
                        player_id,
                        requester_team_id,
                        holder_team_id,
                        today,
                        recent_block ? "recent_block" : "active_right");
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
        && kbo_team_id_is_military_service_team(requester_team_id)) {
        static LONG military_ai_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_ai_block_log_count);
        if (slot <= 300) {
            append_logf(
                "military service team AI FA status candidate blocked player=%u requester_team=%u index=%d",
                player_id,
                requester_team_id,
                insert_index);
        }
        return insert_index;
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

    if (player_id != 0u
            && requester_team_id != 0u
            && kbo_custom_foreign_policy_enabled()
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)player_ptr)) {
        if (today == 0u && !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            today = 0u;
        }
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            (uint8_t*)player_ptr,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            static LONG custom_policy_ai_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_policy_ai_block_log_count);
            if (slot <= 300) {
                append_logf(
                    "custom foreign policy AI FA status candidate blocked player=%u requester_team=%u index=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    requester_team_id,
                    insert_index,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            return insert_index;
        }
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
    return kbo_get_save_scoped_data_file("foreign_waiver_commands.txt", out, out_size);
}

static int get_kbo_foreign_waiver_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_candidates.csv", out, out_size);
}

static int get_kbo_foreign_roster_audit_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_roster_audit.csv", out, out_size);
}

static int get_kbo_foreign_roster_snapshot_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_roster_snapshot.csv", out, out_size);
}

static int append_foreign_waiver_candidate_csv_header(HANDLE file)
{
    const char* header = "date,source,player_id,current_team_id,active_team_id,original_team_id,current_league_id,priority_window_open,priority_team_id,priority_eligible,dfa_flag,restricted,secondary_restricted,loan_active,injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static int append_foreign_roster_audit_csv_header(HANDLE file)
{
    const char* header =
        "date,source,change_type,player_id,nation_id,"
        "old_current_team_id,new_current_team_id,"
        "old_active_team_id,new_active_team_id,"
        "old_original_team_id,new_original_team_id,"
        "old_current_league_id,new_current_league_id,"
        "old_loan_team_id,new_loan_team_id,"
        "old_restricted,new_restricted,"
        "old_secondary_restricted,new_secondary_restricted,"
        "old_dfa,new_dfa,"
        "old_loan_active,new_loan_active,"
        "old_injury_active,new_injury_active,"
        "old_foreign_value_score,new_foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static int append_foreign_roster_snapshot_csv_header(HANDLE file)
{
    const char* header =
        "date,source,player_id,nation_id,current_team_id,active_team_id,original_team_id,"
        "current_league_id,loan_team_id,restricted,secondary_restricted,dfa,loan_active,"
        "injury_active,foreign_value_score\r\n";
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

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128
#define KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES 40

typedef struct KboForeignReplacementPlayerSeed {
    char key[KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES];
    uint32_t player_id;
    uint8_t slot_type;
} KboForeignReplacementPlayerSeed;

static KboForeignReplacementPlayerSeed g_kbo_foreign_replacement_player_seeds[KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX];
static int g_kbo_foreign_replacement_player_seed_count = 0;
static char g_kbo_foreign_replacement_player_seed_loaded_path[MAX_PATH] = {0};
static LONG g_kbo_foreign_replacement_player_seed_lock = 0;
static LONG64 g_kbo_foreign_replacement_player_seed_last_resolve_tick = 0;

static int kbo_persist_foreign_replacement_player_resolved_cache_locked(void);

static int kbo_get_save_foreign_replacement_players_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_replacement_players_seed.csv", out, out_size);
}

static int kbo_get_global_foreign_replacement_players_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_replacement_players_seed.csv", local_app_data);
    return out[0] != '\0';
}

static int kbo_get_save_foreign_replacement_players_resolved_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_replacement_players_resolved.csv", out, out_size);
}

static int kbo_get_current_players_dat_path(char* out, size_t out_size)
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

static void kbo_lock_foreign_replacement_player_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_replacement_player_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_foreign_replacement_player_seeds(void)
{
    InterlockedExchange(&g_kbo_foreign_replacement_player_seed_lock, 0);
}

static int kbo_ascii_is_seed_id_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static void kbo_trim_csv_token_in_place(char* text)
{
    if (text == NULL) {
        return;
    }
    char* start = text;
    while (*start == ' ' || *start == '\t' || *start == '"') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n' || end[-1] == '"')) {
        end--;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }
}

static uint8_t kbo_parse_foreign_replacement_seed_slot_type(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    if (text[0] == '2') {
        return KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
    }
    if (text[0] == '1') {
        return KBO_FOREIGN_INJURY_SLOT_REGULAR;
    }
    if (_stricmp(text, "asian") == 0 || _stricmp(text, "asian_quota") == 0 || _stricmp(text, "asia") == 0) {
        return KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
    }
    if (_stricmp(text, "regular") == 0 || _stricmp(text, "foreign") == 0) {
        return KBO_FOREIGN_INJURY_SLOT_REGULAR;
    }
    return 0u;
}

static int kbo_parse_foreign_replacement_player_seed_line(
    const char* line,
    KboForeignReplacementPlayerSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[160] = {0};
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

    char* first = p;
    char* comma = strchr(first, ',');
    char* second = NULL;
    if (comma != NULL) {
        *comma = '\0';
        second = comma + 1;
        char* second_comma = strchr(second, ',');
        if (second_comma != NULL) {
            *second_comma = '\0';
        }
    }

    kbo_trim_csv_token_in_place(first);
    if (second != NULL) {
        kbo_trim_csv_token_in_place(second);
    }
    if (first[0] == '\0' || _stricmp(first, "replacement_player_key") == 0 || _stricmp(first, "player_id") == 0) {
        return 0;
    }

    snprintf(out->key, sizeof(out->key), "%s", first);
    out->slot_type = kbo_parse_foreign_replacement_seed_slot_type(second);
    if (first[0] >= '0' && first[0] <= '9') {
        out->player_id = (uint32_t)strtoul(first, NULL, 10);
    }
    return out->key[0] != '\0' || out->player_id != 0u;
}

static int kbo_player_memory_contains_ascii_seed_key(uint8_t* player, const char* key)
{
    if (player == NULL || key == NULL || key[0] == '\0' || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    size_t key_len = strlen(key);
    if (key_len < 3u || key_len >= KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES || key_len >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    for (size_t i = 0; i + key_len < OOTP27_PLAYER_SCAN_BYTES; i++) {
        if (memcmp(player + i, key, key_len) != 0) {
            continue;
        }
        char before = i > 0u ? (char)player[i - 1u] : '\0';
        char after = (char)player[i + key_len];
        if (!kbo_ascii_is_seed_id_char(before) && !kbo_ascii_is_seed_id_char(after)) {
            return 1;
        }
    }
    return 0;
}

static uint32_t kbo_resolve_foreign_replacement_player_seed_key(const char* key, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
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
    uint8_t first_slot_type = 0u;
    int matches = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_player_memory_contains_ascii_seed_key(player, key)) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        if (first_match == 0u) {
            first_match = player_id;
            first_slot_type = kbo_player_is_asian_quota_candidate(player)
                ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                : KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
        matches++;
    }

    if (first_match != 0u) {
        if (out_slot_type != NULL) {
            *out_slot_type = first_slot_type;
        }
        append_logf(
            "foreign replacement player seed resolved key=%s player=%u matches=%d slot=%s",
            key,
            first_match,
            matches,
            first_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular");
    }
    return first_match;
}

static int kbo_buffer_contains_u32_le(const uint8_t* data, size_t size, uint32_t value)
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

static uint32_t kbo_foreign_resolve_player_id_from_players_dat_record_start(
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
                    && year >= 1800u && year <= 2100u) {
                uint8_t* candidate = kbo_find_player_by_id(candidate_id, NULL, NULL);
                if (candidate != NULL && kbo_player_pointer_plausible((uintptr_t)candidate)) {
                    return candidate_id;
                }
            }
        }
        if (start == min_start || start == 0u) {
            break;
        }
    }
    return 0u;
}

static uint32_t kbo_resolve_foreign_replacement_player_seed_from_players_dat(const char* key, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (key == NULL || key[0] == '\0' || (key[0] >= '0' && key[0] <= '9')) {
        return 0u;
    }

    char players_dat_path[MAX_PATH] = {0};
    if (!kbo_get_current_players_dat_path(players_dat_path, sizeof(players_dat_path))) {
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat missing or save not written", key);
        return 0u;
    }

    HANDLE file = CreateFileA(players_dat_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat open failed gle=%lu", key, (unsigned long)GetLastError());
        return 0u;
    }

    LARGE_INTEGER file_size;
    file_size.QuadPart = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(file);
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat size unsupported", key);
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
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat read failed", key);
        return 0u;
    }

    size_t key_len = strlen(key);
    uint32_t matched_player_id = 0u;
    uint8_t matched_slot_type = 0u;
    int ambiguous = 0;
    for (size_t pos = 0; pos + key_len <= (size_t)read; pos++) {
        if (memcmp(raw + pos, key, key_len) != 0) {
            continue;
        }
        char before = pos > 0u ? (char)raw[pos - 1u] : '\0';
        char after = (char)raw[pos + key_len];
        if (kbo_ascii_is_seed_id_char(before) || kbo_ascii_is_seed_id_char(after)) {
            continue;
        }

        uint32_t record_player_id = kbo_foreign_resolve_player_id_from_players_dat_record_start(raw, (size_t)read, pos);
        if (record_player_id != 0u) {
            uint8_t* candidate = kbo_find_player_by_id(record_player_id, NULL, NULL);
            if (matched_player_id != 0u && matched_player_id != record_player_id) {
                ambiguous = 1;
                break;
            }
            matched_player_id = record_player_id;
            matched_slot_type = candidate != NULL && kbo_player_is_asian_quota_candidate(candidate)
                ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                : KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (ambiguous) {
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat ambiguous", key);
        return 0u;
    }
    if (matched_player_id != 0u) {
        if (out_slot_type != NULL) {
            *out_slot_type = matched_slot_type;
        }
        append_logf(
            "foreign replacement player seed resolved via players.dat key=%s player=%u slot=%s",
            key,
            matched_player_id,
            matched_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular");
    }
    return matched_player_id;
}

static int kbo_add_foreign_replacement_player_seed_locked(const KboForeignReplacementPlayerSeed* seed)
{
    if (seed == NULL || (seed->key[0] == '\0' && seed->player_id == 0u)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* existing = &g_kbo_foreign_replacement_player_seeds[i];
        if ((seed->player_id != 0u && existing->player_id == seed->player_id)
                || (seed->key[0] != '\0' && existing->key[0] != '\0' && _stricmp(existing->key, seed->key) == 0)) {
            if (existing->slot_type == 0u && seed->slot_type != 0u) {
                existing->slot_type = seed->slot_type;
            }
            return 0;
        }
    }
    if (g_kbo_foreign_replacement_player_seed_count >= KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX) {
        return 0;
    }
    g_kbo_foreign_replacement_player_seeds[g_kbo_foreign_replacement_player_seed_count++] = *seed;
    return 1;
}

static int kbo_import_foreign_replacement_player_seed_file_locked(const char* path, const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
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

            char line[180] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            KboForeignReplacementPlayerSeed seed;
            if (kbo_parse_foreign_replacement_player_seed_line(line, &seed)
                    && kbo_add_foreign_replacement_player_seed_locked(&seed)) {
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
            "foreign replacement player seed import source=%s imported=%d path=%s",
            source != NULL ? source : "",
            imported,
            path);
    }
    return imported;
}

static int kbo_load_foreign_replacement_player_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_resolved_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
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
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[160] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            char* key = line;
            char* comma = strchr(key, ',');
            if (comma != NULL) {
                *comma = '\0';
                char* player_text = comma + 1;
                char* second_comma = strchr(player_text, ',');
                char* slot_text = NULL;
                if (second_comma != NULL) {
                    *second_comma = '\0';
                    slot_text = second_comma + 1;
                    char* third_comma = strchr(slot_text, ',');
                    if (third_comma != NULL) {
                        *third_comma = '\0';
                    }
                }
                kbo_trim_csv_token_in_place(key);
                kbo_trim_csv_token_in_place(player_text);
                if (slot_text != NULL) {
                    kbo_trim_csv_token_in_place(slot_text);
                }
                if (key[0] != '\0' && _stricmp(key, "source_key") != 0 && player_text[0] >= '0' && player_text[0] <= '9') {
                    uint32_t player_id = (uint32_t)strtoul(player_text, NULL, 10);
                    uint8_t slot_type = kbo_parse_foreign_replacement_seed_slot_type(slot_text);
                    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
                        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
                        if (seed->key[0] != '\0' && _stricmp(seed->key, key) == 0) {
                            seed->player_id = player_id;
                            if (seed->slot_type == 0u) {
                                seed->slot_type = slot_type;
                            }
                            loaded++;
                            break;
                        }
                    }
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
    if (loaded > 0) {
        append_logf("foreign replacement player resolved cache loaded=%d path=%s", loaded, path);
    }
    return loaded;
}

static int kbo_persist_foreign_replacement_player_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_resolved_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign replacement player resolved cache persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD wrote = 0;
    const char* header = "source_key,player_id,slot_type\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &wrote, NULL);
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->key[0] == '\0' || seed->player_id == 0u) {
            continue;
        }
        char line[160] = {0};
        int len = snprintf(line, sizeof(line), "%s,%u,%u\r\n", seed->key, seed->player_id, (uint32_t)seed->slot_type);
        if (len > 0 && len < (int)sizeof(line)) {
            WriteFile(file, line, (DWORD)len, &wrote, NULL);
        }
    }
    CloseHandle(file);
    return 1;
}

static int kbo_resolve_foreign_replacement_player_seeds_locked(void)
{
    int resolved = 0;
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->player_id != 0u) {
            continue;
        }
        uint8_t resolved_slot_type = 0u;
        uint32_t player_id = 0u;
        if (seed->key[0] >= '0' && seed->key[0] <= '9') {
            player_id = (uint32_t)strtoul(seed->key, NULL, 10);
        } else {
            player_id = kbo_resolve_foreign_replacement_player_seed_from_players_dat(seed->key, &resolved_slot_type);
        }
        if (player_id != 0u) {
            seed->player_id = player_id;
            if (seed->slot_type == 0u) {
                seed->slot_type = resolved_slot_type;
            }
            resolved++;
        }
    }
    InterlockedExchange64(&g_kbo_foreign_replacement_player_seed_last_resolve_tick, (LONG64)GetTickCount64());
    if (resolved > 0) {
        kbo_persist_foreign_replacement_player_resolved_cache_locked();
    }
    return resolved;
}

static void kbo_ensure_foreign_replacement_player_seeds_loaded(void)
{
    char current_path[MAX_PATH] = {0};
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_seed_path(save_seed_path, sizeof(save_seed_path))) {
        save_seed_path[0] = '\0';
    }
    if (!kbo_get_global_foreign_replacement_players_seed_path(global_seed_path, sizeof(global_seed_path))) {
        global_seed_path[0] = '\0';
    }
    snprintf(current_path, sizeof(current_path), "%s", save_seed_path[0] != '\0' ? save_seed_path : global_seed_path);

    kbo_lock_foreign_replacement_player_seeds();
    if (strcmp(g_kbo_foreign_replacement_player_seed_loaded_path, current_path) != 0) {
        memset(g_kbo_foreign_replacement_player_seeds, 0, sizeof(g_kbo_foreign_replacement_player_seeds));
        g_kbo_foreign_replacement_player_seed_count = 0;
        snprintf(g_kbo_foreign_replacement_player_seed_loaded_path, sizeof(g_kbo_foreign_replacement_player_seed_loaded_path), "%s", current_path);
        if (save_seed_path[0] != '\0') {
            kbo_import_foreign_replacement_player_seed_file_locked(save_seed_path, "save_seed");
        }
        if (global_seed_path[0] != '\0') {
            kbo_import_foreign_replacement_player_seed_file_locked(global_seed_path, "global_seed");
        }
        kbo_load_foreign_replacement_player_resolved_cache_locked();
        kbo_resolve_foreign_replacement_player_seeds_locked();
    } else {
        int unresolved = 0;
        for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
            if (g_kbo_foreign_replacement_player_seeds[i].player_id == 0u) {
                unresolved++;
            }
        }
        LONG64 last_tick = InterlockedCompareExchange64(&g_kbo_foreign_replacement_player_seed_last_resolve_tick, 0, 0);
        ULONGLONG now = GetTickCount64();
        if (unresolved > 0 && (last_tick <= 0 || now < (ULONGLONG)last_tick || now - (ULONGLONG)last_tick > 5000ull)) {
            kbo_resolve_foreign_replacement_player_seeds_locked();
        }
    }
    kbo_unlock_foreign_replacement_player_seeds();
}

static int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    int matched = 0;
    kbo_lock_foreign_replacement_player_seeds();
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->player_id == player_id) {
            if (out_slot_type != NULL) {
                *out_slot_type = seed->slot_type;
            }
            matched = 1;
            break;
        }
    }
    kbo_unlock_foreign_replacement_player_seeds();
    return matched;
}

#define KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS 42
#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4

typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

static KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {{0}};
static int  g_kbo_foreign_injury_replacement_count = 0;
static LONG g_kbo_foreign_injury_replacement_lock = 0;
static char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH] = {0};

static int kbo_persist_foreign_injury_replacements_locked(void);
static int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);

static int kbo_foreign_injury_replacement_enabled(void)
{
    return kbo_fix_enabled() && !read_kbo_localappdata_flag_file("disable_foreign_injury_replacement.txt");
}

static int kbo_get_foreign_injury_replacement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_injury_replacements.csv", out, out_size);
}

static int kbo_get_save_foreign_injury_replacement_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_injury_replacements_seed.csv", out, out_size);
}

static int kbo_get_global_foreign_injury_replacement_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_injury_replacements_seed.csv", local_app_data);
    return out[0] != '\0';
}

static const char* kbo_foreign_injury_slot_label(uint8_t slot_type)
{
    return slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular";
}

static const char* kbo_foreign_injury_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FOREIGN_INJURY_STATUS_OPEN:    return "Open";
    case KBO_FOREIGN_INJURY_STATUS_ACTIVE:  return "Active";
    case KBO_FOREIGN_INJURY_STATUS_PENDING: return "Decision due";
    case KBO_FOREIGN_INJURY_STATUS_CLOSED:  return "Closed";
    default:                                return "Unknown";
    }
}

static int kbo_foreign_injury_status_uses_slot(uint8_t status)
{
    return status == KBO_FOREIGN_INJURY_STATUS_OPEN || status == KBO_FOREIGN_INJURY_STATUS_ACTIVE;
}

static uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player)
{
    return kbo_player_is_asian_quota_candidate(player)
        ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        : KBO_FOREIGN_INJURY_SLOT_REGULAR;
}

static void kbo_lock_foreign_injury_replacements(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_foreign_injury_replacements(void)
{
    InterlockedExchange(&g_kbo_foreign_injury_replacement_lock, 0);
}

static int kbo_load_foreign_injury_replacements_locked(const char* path)
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

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    append_logf("foreign injury replacement: loaded=%d path=%s", loaded, path);
    return loaded;
}

static int kbo_parse_foreign_injury_replacement_seed_line(
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

static int kbo_import_foreign_injury_replacement_seed_file_locked(
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
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (imported > 0 || skipped > 0) {
        append_logf(
            "foreign injury replacement: seed import source=%s imported=%d skipped=%d path=%s",
            source != NULL ? source : "",
            imported,
            skipped,
            path);
    }
    return imported;
}

static void kbo_ensure_foreign_injury_replacements_loaded(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return;
    }

    kbo_lock_foreign_injury_replacements();
    if (strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) != 0) {
        kbo_load_foreign_injury_replacements_locked(path);
        uint32_t today = 0u;
        kbo_get_current_yyyymmdd(&today);
        int imported = 0;
        char save_seed_path[MAX_PATH] = {0};
        char global_seed_path[MAX_PATH] = {0};
        if (kbo_get_save_foreign_injury_replacement_seed_path(save_seed_path, sizeof(save_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(save_seed_path, today, "save_seed");
        }
        if (kbo_get_global_foreign_injury_replacement_seed_path(global_seed_path, sizeof(global_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(global_seed_path, today, "global_seed");
        }
        if (imported > 0) {
            kbo_persist_foreign_injury_replacements_locked();
        }
    }
    kbo_unlock_foreign_injury_replacements();
}

static int kbo_persist_foreign_injury_replacements_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign injury replacement: persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header = "team_id,league_id,injured_player_id,replacement_player_id,opened_on,expected_end,slot_type,status,converted\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        char line[256] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            rec->team_id,
            rec->league_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            rec->opened_on_yyyymmdd,
            rec->expected_end_yyyymmdd,
            (uint32_t)rec->slot_type,
            (uint32_t)rec->status,
            (uint32_t)rec->converted);
        if (len > 0 && len < (int)sizeof(line)) {
            written = 0;
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    CloseHandle(file);
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);
    return 1;
}

static int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed)
{
    if (injured_player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->injured_player_id == injured_player_id
                && (include_closed || rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED)) {
            return i;
        }
    }
    return -1;
}

static int kbo_team_has_foreign_injury_slot_locked(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    if (out_injured_player_id != NULL) {
        *out_injured_player_id = 0u;
    }
    if (team_id == 0u || slot_type == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id == team_id
                && rec->slot_type == slot_type
                && kbo_foreign_injury_status_uses_slot(rec->status)) {
            if (out_injured_player_id != NULL) {
                *out_injured_player_id = rec->injured_player_id;
            }
            return 1;
        }
    }
    return 0;
}

static int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_locked(team_id, slot_type, out_injured_player_id);
    kbo_unlock_foreign_injury_replacements();
    return result;
}

static int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || slot_type == 0u || candidate_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id != team_id
                || rec->slot_type != slot_type
                || !kbo_foreign_injury_status_uses_slot(rec->status)
                || rec->injured_player_id == candidate_player_id) {
            continue;
        }
        if (rec->replacement_player_id != 0u && rec->replacement_player_id != candidate_player_id) {
            continue;
        }
        if (out_injured_player_id != NULL) {
            *out_injured_player_id = rec->injured_player_id;
        }
        if (out_replacement_player_id != NULL) {
            *out_replacement_player_id = rec->replacement_player_id;
        }
        return 1;
    }
    return 0;
}

static int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_for_candidate_locked(
        team_id,
        slot_type,
        candidate_player_id,
        out_injured_player_id,
        out_replacement_player_id);
    kbo_unlock_foreign_injury_replacements();
    return result;
}

static void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed)
{
    if (out_open != NULL) { *out_open = 0; }
    if (out_pending != NULL) { *out_pending = 0; }
    if (out_closed != NULL) { *out_closed = 0; }
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (team_id != 0u && rec->team_id != team_id) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)) {
            if (out_open != NULL) { (*out_open)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            if (out_pending != NULL) { (*out_pending)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            if (out_closed != NULL) { (*out_closed)++; }
        }
    }
    kbo_unlock_foreign_injury_replacements();
}

static void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u || rec->league_id == 0u) {
        return;
    }

    uint32_t event_date = 0u;
    if (!kbo_get_current_yyyymmdd(&event_date) || event_date == 0u) {
        event_date = rec->opened_on_yyyymmdd;
    }
    if (event_date == 0u) {
        return;
    }

    char player_name[96] = {0};
    uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
    if (player != NULL) {
        kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
    }

    char title[128] = {0};
    char body[1024] = {0};
    if (phase != NULL && strcmp(phase, "pending") == 0) {
        snprintf(title, sizeof(title), "[KBO] Foreign Injury Replacement Decision Required");
        snprintf(
            body,
            sizeof(body),
            "The temporary foreign-player injury replacement window for <Team #%u:team#%u> has moved to a decision stage because <%s:player#%u> is no longer listed as unavailable.\n\nThe club must now close the temporary window or convert the replacement into a regular foreign-player slot under the KBO roster limit.",
            rec->team_id,
            rec->team_id,
            player_name,
            rec->injured_player_id);
    } else {
        snprintf(title, sizeof(title), "[KBO] Foreign Injury Replacement Window Opened");
        snprintf(
            body,
            sizeof(body),
            "The KBO approved a temporary foreign-player injury replacement window for <Team #%u:team#%u> after <%s:player#%u> was diagnosed with an injury expected to keep him out for %d days.\n\nThe club may carry one additional %s foreign player while the injured player remains unavailable.",
            rec->team_id,
            rec->team_id,
            player_name,
            rec->injured_player_id,
            days_left > 0 ? days_left : 0,
            kbo_foreign_injury_slot_label(rec->slot_type));
    }

    int created = create_kbo_native_live_news_with_body(
        event_date / 10000u,
        (event_date / 100u) % 100u,
        event_date % 100u,
        rec->league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "foreign injury replacement: news phase=%s team=%u injured=%u league=%u created=%d",
        phase != NULL ? phase : "open",
        rec->team_id,
        rec->injured_player_id,
        rec->league_id,
        created);
}

static int kbo_foreign_injury_replacement_signing_exception_available(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_effective_count,
    uint32_t* out_effective_limit)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_effective_count != NULL) { *out_effective_count = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = 3u; }

    if (!kbo_foreign_injury_replacement_enabled()
            || team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    if (out_effective_count != NULL) { *out_effective_count = effective; }
    if (out_effective_limit != NULL) { *out_effective_limit = 4u; }

    int candidate_asian = kbo_player_is_asian_quota_candidate(candidate);
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    uint32_t injured_player_id = 0u;
    if (candidate_asian
            && asian_count <= 1u
            && kbo_team_has_foreign_injury_slot_for_candidate(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                candidate_id,
                &injured_player_id,
                NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1;
    }

    if (effective <= 3u
            && kbo_team_has_foreign_injury_slot_for_candidate(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_REGULAR,
                candidate_id,
                &injured_player_id,
                NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1;
    }

    return 0;
}

static int kbo_foreign_injury_replacement_callup_exception_available(
    uintptr_t team_ptr,
    uint8_t* candidate,
    int32_t limit,
    int check_type,
    uint32_t* out_effective_after,
    uint8_t* out_slot_type)
{
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (!kbo_foreign_injury_replacement_enabled()
            || team_ptr == 0
            || limit <= 0
            || candidate == NULL
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    if (team_id == 0u) {
        return 0;
    }

    int candidate_pitcher = (*(uint8_t*)(candidate + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    int candidate_asian = kbo_player_is_asian_quota_candidate(candidate);
    uint8_t slot_type = candidate_asian ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA : KBO_FOREIGN_INJURY_SLOT_REGULAR;
    uint32_t injured_player_id = 0u;
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (!kbo_team_has_foreign_injury_slot_for_candidate(team_id, slot_type, candidate_id, &injured_player_id, NULL)) {
        if (slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                && !kbo_team_has_foreign_injury_slot_for_candidate(
                    team_id,
                    KBO_FOREIGN_INJURY_SLOT_REGULAR,
                    candidate_id,
                    &injured_player_id,
                    NULL)) {
            return 0;
        }
        slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    if (check_type == 1 && candidate_pitcher) {
        return 0;
    }
    if (check_type == 2 && !candidate_pitcher) {
        return 0;
    }

    uint32_t asian_after = asian_hitters + asian_pitchers;
    uint32_t non_asian_after = non_asian_hitters + non_asian_pitchers;
    if (check_type == 1) {
        asian_after = asian_hitters + (!candidate_pitcher && candidate_asian ? 1u : 0u);
        non_asian_after = non_asian_hitters + (!candidate_pitcher && !candidate_asian ? 1u : 0u);
    } else if (check_type == 2) {
        asian_after = asian_pitchers + (candidate_pitcher && candidate_asian ? 1u : 0u);
        non_asian_after = non_asian_pitchers + (candidate_pitcher && !candidate_asian ? 1u : 0u);
    } else {
        asian_after += candidate_asian ? 1u : 0u;
        non_asian_after += candidate_asian ? 0u : 1u;
    }

    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    return effective_after <= ((uint32_t)limit + 1u);
}

static uint32_t kbo_custom_foreign_policy_extra_slots_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    if (!kbo_foreign_injury_replacement_enabled()
            || team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0u;
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (candidate_id == 0u) {
        return 0u;
    }

    uint32_t injured_player_id = 0u;
    if (kbo_player_is_asian_quota_candidate(candidate)
            && kbo_team_has_foreign_injury_slot_for_candidate(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                candidate_id,
                &injured_player_id,
                NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1u;
    }

    if (kbo_team_has_foreign_injury_slot_for_candidate(
            team_id,
            KBO_FOREIGN_INJURY_SLOT_REGULAR,
            candidate_id,
            &injured_player_id,
            NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1u;
    }

    return 0u;
}

static int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id)
{
    if (candidate == NULL || team_id == 0u || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    return current_team_id == 0u || kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
}

static int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    (void)foreign_count;

    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
    if (!already_in_org) {
        if (kbo_player_is_asian_quota_candidate(candidate)) {
            asian_after++;
        } else {
            non_asian_after++;
        }
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        team_id,
        candidate,
        &slot_type,
        &injured_player_id);
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;

    if (out_effective_before != NULL) { *out_effective_before = effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }

    if (already_in_org) {
        return 1;
    }
    return effective_after <= effective_limit;
}

static void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    if (out_foreign_count != NULL) { *out_foreign_count = 0u; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = 0u; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = 0u; }
    if (team_id == 0u) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            static volatile LONG replacement_seed_count_log_count = 0;
            LONG slot = InterlockedIncrement(&replacement_seed_count_log_count);
            if (slot <= 120) {
                append_logf(
                    "foreign replacement player seed excluded from org count team=%u player=%u key_slot=%s nation=%u",
                    team_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    replacement_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular",
                    nation_id);
            }
            continue;
        }
        foreign_count++;
        if (kbo_nation_is_asian_quota_candidate(nation_id)) {
            asian_count++;
        } else {
            non_asian_count++;
        }
    }

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
}

static uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count)
{
    return non_asian_foreign_count + (asian_count > 0u ? asian_count - 1u : 0u);
}

static void kbo_count_active_asian_quota_by_position(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers)
{
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        uint32_t player_id = active_ids[i];
        if (player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (!kbo_nation_is_asian_quota_candidate(nation_id)) {
            continue;
        }

        if (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u) {
            asian_pitchers++;
        } else {
            asian_hitters++;
        }
    }

    if (out_asian_hitters != NULL) { *out_asian_hitters = asian_hitters; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = asian_pitchers; }
}

static int32_t kbo_apply_active_asian_quota_count_exception(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    kbo_count_active_asian_quota_by_position(team_ptr, &asian_hitters, &asian_pitchers);

    int use_exception = 0;
    if (pitcher_count) {
        use_exception = (asian_pitchers > 0u && asian_hitters == 0u);
    } else {
        use_exception = (asian_hitters > 0u);
    }
    if (!use_exception) {
        return original_count;
    }

    int32_t adjusted = original_count - 1;
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        append_logf(
            "asian quota active foreign slot exception team=%u type=%s original=%d adjusted=%d asian_hitters=%u asian_pitchers=%u",
            team_id,
            pitcher_count ? "pitcher" : "hitter",
            original_count,
            adjusted,
            asian_hitters,
            asian_pitchers);
    }
    return adjusted;
}

static int32_t kbo_custom_foreign_policy_neutralized_count(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(
        asian_hitters + asian_pitchers,
        non_asian_hitters + non_asian_pitchers);

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        append_logf(
            "custom foreign policy neutralized OOTP active count team=%u type=%s original=%d adjusted=0 active_effective=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            pitcher_count ? "pitcher" : "hitter",
            original_count,
            effective,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return 0;
}

__declspec(noinline) int32_t ootp_kbo_active_foreign_hitter_count_wrapper(
    uintptr_t team_ptr,
    uintptr_t original_func_ptr)
{
    typedef int32_t (__fastcall *OriginalFn)(uintptr_t);
    int32_t original = ((OriginalFn)original_func_ptr)(team_ptr);
    if (kbo_custom_foreign_policy_enabled()) {
        return kbo_custom_foreign_policy_neutralized_count(team_ptr, original, 0);
    }
    return kbo_apply_active_asian_quota_count_exception(team_ptr, original, 0);
}

__declspec(noinline) int32_t ootp_kbo_active_foreign_pitcher_count_wrapper(
    uintptr_t team_ptr,
    uintptr_t original_func_ptr)
{
    typedef int32_t (__fastcall *OriginalFn)(uintptr_t);
    int32_t original = ((OriginalFn)original_func_ptr)(team_ptr);
    if (kbo_custom_foreign_policy_enabled()) {
        return kbo_custom_foreign_policy_neutralized_count(team_ptr, original, 1);
    }
    return kbo_apply_active_asian_quota_count_exception(team_ptr, original, 1);
}

static void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers)
{
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = 0u; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = 0u; }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        uint32_t player_id = active_ids[i];
        if (player_id == 0u) {
            continue;
        }
        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        int pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
        int asian = kbo_nation_is_asian_quota_candidate(nation_id);
        if (pitcher) {
            if (asian) { asian_pitchers++; } else { non_asian_pitchers++; }
        } else {
            if (asian) { asian_hitters++; } else { non_asian_hitters++; }
        }
    }

    if (out_asian_hitters != NULL) { *out_asian_hitters = asian_hitters; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = asian_pitchers; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = non_asian_hitters; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = non_asian_pitchers; }
}

static int kbo_team_active_roster_contains_player(uintptr_t team_ptr, uint32_t player_id)
{
    if (team_ptr == 0
            || player_id == 0u
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type)
{
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0u;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 1u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    int candidate_pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    if (check_type == 1 && candidate_pitcher) {
        return 1u;
    }
    if (check_type == 2 && !candidate_pitcher) {
        return 1u;
    }

    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_id == 0u || player_id == 0u) {
        return 0u;
    }
    if (kbo_team_active_roster_contains_player(team_ptr, player_id)) {
        return 1u;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    uint32_t asian_before = asian_hitters + asian_pitchers;
    uint32_t non_asian_before = non_asian_hitters + non_asian_pitchers;
    uint32_t asian_after = asian_before;
    uint32_t non_asian_after = non_asian_before;
    if (kbo_player_is_asian_quota_candidate(player)) {
        asian_after++;
    } else {
        non_asian_after++;
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        team_id,
        player,
        &slot_type,
        &injured_player_id);
    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_before, non_asian_before);
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;
    uint8_t allowed = effective_after <= effective_limit ? 1u : 0u;

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 240) {
        append_logf(
            "custom foreign policy callup team=%u player=%u type=%s active_count=%d ootp_limit=%d allowed=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            player_id,
            check_type == 1 ? "hitter" : (check_type == 2 ? "pitcher" : "total"),
            active_count,
            ootp_limit,
            (uint32_t)allowed,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return allowed;
}

static uint8_t kbo_callup_foreign_limit_allows_with_asian_quota(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t limit,
    int check_type)
{
    if (kbo_custom_foreign_policy_enabled()) {
        return kbo_custom_foreign_policy_callup_allows(team_ptr, player_ptr, active_count, limit, check_type);
    }
    if (active_count < limit) {
        return 1u;
    }
    if (limit <= 0 || player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t injury_effective_after = 0u;
    uint8_t injury_slot_type = 0u;
    if (kbo_foreign_injury_replacement_callup_exception_available(
            team_ptr,
            player,
            limit,
            check_type,
            &injury_effective_after,
            &injury_slot_type)) {
        static volatile LONG injury_log_count = 0;
        LONG injury_slot = InterlockedIncrement(&injury_log_count);
        if (injury_slot <= 160) {
            uint32_t team_id = 0u;
            if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
            }
            append_logf(
                "foreign injury replacement callup exception team=%u player=%u type=%s active_count=%d limit=%d effective_after=%u",
                team_id,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                kbo_foreign_injury_slot_label(injury_slot_type),
                active_count,
                limit,
                injury_effective_after);
        }
        return 1u;
    }
    if (!kbo_nation_is_asian_quota_candidate(nation_id)) {
        return 0u;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    int candidate_pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    uint32_t effective_after = 0u;
    if (check_type == 1) {
        uint32_t asian_after = asian_hitters + (candidate_pitcher ? 0u : 1u);
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_hitters);
    } else if (check_type == 2) {
        uint32_t asian_after = asian_pitchers + (candidate_pitcher ? 1u : 0u);
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_pitchers);
    } else {
        uint32_t asian_after = asian_hitters + asian_pitchers + 1u;
        uint32_t non_asian_after = non_asian_hitters + non_asian_pitchers;
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    }

    uint8_t allowed = (effective_after <= (uint32_t)limit) ? 1u : 0u;
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        append_logf(
            "asian quota callup limit check team=%u player=%u nation=%u type=%s active_count=%d limit=%d effective_after=%u allowed=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
            nation_id,
            check_type == 1 ? "hitter" : (check_type == 2 ? "pitcher" : "total"),
            active_count,
            limit,
            effective_after,
            (uint32_t)allowed,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return allowed;
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_hitter_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    return kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 1);
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    return kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 2);
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_total_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    return kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 3);
}

static int kbo_asian_quota_exception_available_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_team_foreign,
    uint32_t* out_team_asian,
    uint32_t* out_team_non_asian,
    uint32_t* out_team_effective,
    uint32_t* out_effective_after)
{
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);

    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    int candidate_is_asian = kbo_player_is_asian_quota_candidate(candidate);
    int exception_available = candidate_is_asian && asian_count == 0u;
    uint32_t effective_after = effective;
    if (!exception_available) {
        effective_after++;
    }

    if (out_team_foreign != NULL) { *out_team_foreign = foreign_count; }
    if (out_team_asian != NULL) { *out_team_asian = asian_count; }
    if (out_team_non_asian != NULL) { *out_team_non_asian = non_asian_count; }
    if (out_team_effective != NULL) { *out_team_effective = effective; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    return exception_available;
}

static void kbo_log_asian_quota_signability_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    int original_signability,
    uintptr_t caller_rva)
{
    if (player == NULL || player_id == 0u || team_id <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 240) {
        return;
    }

    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    uint32_t team_effective = 0u;
    uint32_t effective_after = 0u;
    int exception_available = kbo_asian_quota_exception_available_for_candidate(
        (uint32_t)team_id,
        player,
        &team_foreign,
        &team_asian,
        &team_non_asian,
        &team_effective,
        &effective_after);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);

    append_logf(
        "asian quota signability probe player=%u requester_team=%d nation=%u asian_candidate=%d exception_available=%d"
        " original=%d team_foreign_raw=%u team_asian=%u team_non_asian=%u team_effective_foreign=%u effective_after=%u current_team=%u active_team=%u caller_rva=0x%llx",
        player_id,
        team_id,
        nation_id,
        kbo_player_is_asian_quota_candidate(player),
        exception_available,
        original_signability,
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective,
        effective_after,
        current_team_id,
        active_team_id,
        (unsigned long long)caller_rva);
}

static void kbo_log_asian_quota_offer_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    uint8_t original_result,
    int32_t flag)
{
    if (player == NULL || player_id == 0u || team_id <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 160) {
        return;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_after = 0u;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    int exception_available = kbo_asian_quota_exception_available_for_candidate(
        (uint32_t)team_id,
        player,
        &foreign_count,
        &asian_count,
        &non_asian_count,
        &effective_count,
        &effective_after);

    append_logf(
        "asian quota offer probe player=%u requester_team=%d nation=%u asian_candidate=%d original=%u flag=%d"
        " exception_available=%d team_foreign_raw=%u team_asian=%u team_non_asian_foreign=%u team_effective_foreign=%u effective_after=%u current_team=%u active_team=%u current_league=%u today=%u",
        player_id,
        team_id,
        nation_id,
        kbo_nation_is_asian_quota_candidate(nation_id),
        (uint32_t)original_result,
        flag,
        exception_available,
        foreign_count,
        asian_count,
        non_asian_count,
        effective_count,
        effective_after,
        current_team_id,
        active_team_id,
        current_league_id,
        today);
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

static void kbo_capture_foreign_roster_audit_state(uint8_t* player, KboForeignRosterAuditState* out)
{
    memset(out, 0, sizeof(*out));
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->original_team_id = kbo_get_player_original_team_id(player);
    out->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    out->loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    out->restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    out->secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    out->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    out->loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    out->injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    out->score = kbo_foreign_waiver_value_score(player);
}

static int kbo_foreign_roster_audit_state_changed(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    return old_state->nation_id != new_state->nation_id
        || old_state->current_team_id != new_state->current_team_id
        || old_state->active_team_id != new_state->active_team_id
        || old_state->original_team_id != new_state->original_team_id
        || old_state->current_league_id != new_state->current_league_id
        || old_state->loan_team_id != new_state->loan_team_id
        || old_state->restricted != new_state->restricted
        || old_state->secondary_restricted != new_state->secondary_restricted
        || old_state->dfa != new_state->dfa
        || old_state->loan_active != new_state->loan_active
        || old_state->injury_active != new_state->injury_active
        || old_state->score != new_state->score;
}

static const char* kbo_foreign_roster_audit_change_type(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (old_state == NULL) {
        return "NEW_FOREIGN";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u
            && old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "RELEASE_OBSERVED";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u) {
        return "CURRENT_TEAM_CLEARED";
    }
    if (old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "ACTIVE_TEAM_CLEARED";
    }
    if (old_state->current_team_id == 0u && new_state->current_team_id != 0u) {
        return "CURRENT_TEAM_ASSIGNED";
    }
    if (old_state->restricted != new_state->restricted
            || old_state->secondary_restricted != new_state->secondary_restricted
            || old_state->dfa != new_state->dfa
            || old_state->loan_active != new_state->loan_active
            || old_state->injury_active != new_state->injury_active) {
        return "STATUS_CHANGED";
    }
    if (old_state->current_league_id != new_state->current_league_id
            || old_state->loan_team_id != new_state->loan_team_id) {
        return "ASSIGNMENT_CHANGED";
    }
    return "STATE_CHANGED";
}

static KboForeignRosterAuditState* kbo_find_foreign_roster_audit_state(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
        if (g_kbo_foreign_roster_audit[i].player_id == player_id) {
            return &g_kbo_foreign_roster_audit[i];
        }
    }
    return NULL;
}

static HANDLE kbo_open_foreign_roster_audit_append_file(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_roster_audit_csv_path(path, sizeof(path))) {
        append_log_line("foreign roster audit: unable to resolve audit output path");
        return INVALID_HANDLE_VALUE;
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign roster audit: failed to open audit file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    if (needs_header || is_csv_empty(file)) {
        append_foreign_roster_audit_csv_header(file);
    }
    return file;
}

static void kbo_write_foreign_roster_audit_change(
    HANDLE file,
    const char* date,
    const char* source,
    const char* change_type,
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (file == INVALID_HANDLE_VALUE || new_state == NULL) {
        return;
    }

    KboForeignRosterAuditState zero = {0};
    if (old_state == NULL) {
        old_state = &zero;
    }

    char line[512] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        change_type != NULL ? change_type : "",
        new_state->player_id,
        new_state->nation_id,
        old_state->current_team_id,
        new_state->current_team_id,
        old_state->active_team_id,
        new_state->active_team_id,
        old_state->original_team_id,
        new_state->original_team_id,
        old_state->current_league_id,
        new_state->current_league_id,
        old_state->loan_team_id,
        new_state->loan_team_id,
        (uint32_t)old_state->restricted,
        (uint32_t)new_state->restricted,
        (uint32_t)old_state->secondary_restricted,
        (uint32_t)new_state->secondary_restricted,
        (uint32_t)old_state->dfa,
        (uint32_t)new_state->dfa,
        (uint32_t)old_state->loan_active,
        (uint32_t)new_state->loan_active,
        (uint32_t)old_state->injury_active,
        (uint32_t)new_state->injury_active,
        old_state->score,
        new_state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static void kbo_write_foreign_roster_snapshot_row(
    HANDLE file,
    const char* date,
    const char* source,
    const KboForeignRosterAuditState* state)
{
    if (file == INVALID_HANDLE_VALUE || state == NULL) {
        return;
    }

    char line[320] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        state->player_id,
        state->nation_id,
        state->current_team_id,
        state->active_team_id,
        state->original_team_id,
        state->current_league_id,
        state->loan_team_id,
        (uint32_t)state->restricted,
        (uint32_t)state->secondary_restricted,
        (uint32_t)state->dfa,
        (uint32_t)state->loan_active,
        (uint32_t)state->injury_active,
        state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static HANDLE kbo_open_foreign_roster_snapshot_file(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_roster_snapshot_csv_path(path, sizeof(path))) {
        append_log_line("foreign roster audit: unable to resolve snapshot output path");
        return INVALID_HANDLE_VALUE;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign roster audit: failed to open snapshot file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    append_foreign_roster_snapshot_csv_header(file);
    return file;
}

static void audit_foreign_roster_state(const char* source, int write_snapshot)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            append_log_line("foreign roster audit: no player vector");
        }
        return;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        strcpy_s(save_path, sizeof(save_path), "unknown_save");
    }
    if (strcmp(g_kbo_foreign_roster_audit_save_path, save_path) != 0) {
        g_kbo_foreign_roster_audit_count = 0;
        g_kbo_foreign_roster_audit_generation = 0u;
        memset(g_kbo_foreign_roster_audit, 0, sizeof(g_kbo_foreign_roster_audit));
        snprintf(g_kbo_foreign_roster_audit_save_path, sizeof(g_kbo_foreign_roster_audit_save_path), "%s", save_path);
        append_logf("foreign roster audit: baseline reset save=%s source=%s", save_path, source != NULL ? source : "");
    }

    g_kbo_foreign_roster_audit_generation++;
    if (g_kbo_foreign_roster_audit_generation == 0u) {
        g_kbo_foreign_roster_audit_generation = 1u;
        for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
            g_kbo_foreign_roster_audit[i].seen_generation = 0u;
        }
    }

    int baseline_scan = g_kbo_foreign_roster_audit_count == 0 && g_kbo_foreign_roster_audit_generation == 1u;
    int write_snapshot_now = write_snapshot || baseline_scan;

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    HANDLE audit_file = INVALID_HANDLE_VALUE;
    HANDLE snapshot_file = write_snapshot_now ? kbo_open_foreign_roster_snapshot_file() : INVALID_HANDLE_VALUE;

    int scanned = 0;
    int foreign = 0;
    int rostered = 0;
    int active_retained = 0;
    int free_or_unassigned = 0;
    int changed = 0;
    int new_foreign = 0;
    int current_cleared = 0;
    int active_cleared = 0;
    static volatile LONG release_detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        KboForeignRosterAuditState current = {0};
        kbo_capture_foreign_roster_audit_state(player, &current);
        if (current.player_id == 0u) {
            continue;
        }

        foreign++;
        if (current.current_team_id != 0u) {
            rostered++;
        } else if (current.active_team_id != 0u) {
            active_retained++;
        } else {
            free_or_unassigned++;
        }

        if (snapshot_file != INVALID_HANDLE_VALUE) {
            kbo_write_foreign_roster_snapshot_row(snapshot_file, date, source, &current);
        }

        KboForeignRosterAuditState* previous = kbo_find_foreign_roster_audit_state(current.player_id);
        if (previous == NULL) {
            if (g_kbo_foreign_roster_audit_count < KBO_FOREIGN_ROSTER_AUDIT_MAX) {
                previous = &g_kbo_foreign_roster_audit[g_kbo_foreign_roster_audit_count++];
                *previous = current;
                previous->seen_generation = g_kbo_foreign_roster_audit_generation;
                if (!baseline_scan) {
                    if (audit_file == INVALID_HANDLE_VALUE) {
                        audit_file = kbo_open_foreign_roster_audit_append_file();
                    }
                    kbo_write_foreign_roster_audit_change(
                        audit_file,
                        date,
                        source,
                        "NEW_FOREIGN",
                        NULL,
                        &current);
                    changed++;
                    new_foreign++;
                }
            } else {
                static volatile LONG full_log_count = 0;
                if (InterlockedIncrement(&full_log_count) <= 5) {
                    append_logf(
                        "foreign roster audit: state table full max=%d player=%u",
                        KBO_FOREIGN_ROSTER_AUDIT_MAX,
                        current.player_id);
                }
            }
            continue;
        }

        KboForeignRosterAuditState old = *previous;
        previous->seen_generation = g_kbo_foreign_roster_audit_generation;
        if (!kbo_foreign_roster_audit_state_changed(&old, &current)) {
            continue;
        }

        const char* change_type = kbo_foreign_roster_audit_change_type(&old, &current);
        if (audit_file == INVALID_HANDLE_VALUE) {
            audit_file = kbo_open_foreign_roster_audit_append_file();
        }
        kbo_write_foreign_roster_audit_change(audit_file, date, source, change_type, &old, &current);

        if (old.current_team_id != 0u && current.current_team_id == 0u) {
            current_cleared++;
            LONG slot = InterlockedIncrement(&release_detail_log_count);
            if (slot <= 240) {
                append_logf(
                    "foreign roster audit: %s player=%u nation=%u current=%u->%u active=%u->%u original=%u->%u restricted=%u->%u secondary=%u->%u dfa=%u->%u loan=%u->%u injury=%u->%u score=%d->%d date=%s source=%s",
                    change_type,
                    current.player_id,
                    current.nation_id,
                    old.current_team_id,
                    current.current_team_id,
                    old.active_team_id,
                    current.active_team_id,
                    old.original_team_id,
                    current.original_team_id,
                    (uint32_t)old.restricted,
                    (uint32_t)current.restricted,
                    (uint32_t)old.secondary_restricted,
                    (uint32_t)current.secondary_restricted,
                    (uint32_t)old.dfa,
                    (uint32_t)current.dfa,
                    (uint32_t)old.loan_active,
                    (uint32_t)current.loan_active,
                    (uint32_t)old.injury_active,
                    (uint32_t)current.injury_active,
                    old.score,
                    current.score,
                    date,
                    source != NULL ? source : "");
            }
        }
        if (old.active_team_id != 0u && current.active_team_id == 0u) {
            active_cleared++;
        }

        current.seen_generation = g_kbo_foreign_roster_audit_generation;
        *previous = current;
        changed++;
    }

    if (audit_file != INVALID_HANDLE_VALUE) {
        CloseHandle(audit_file);
    }
    if (snapshot_file != INVALID_HANDLE_VALUE) {
        CloseHandle(snapshot_file);
    }

    if (baseline_scan || changed > 0) {
        append_logf(
            "foreign roster audit: source=%s date=%s scanned=%d foreign=%d rostered=%d active_retained=%d free=%d changed=%d new=%d current_cleared=%d active_cleared=%d snapshot=%d save=%s",
            source != NULL ? source : "",
            date,
            scanned,
            foreign,
            rostered,
            active_retained,
            free_or_unassigned,
            changed,
            new_foreign,
            current_cleared,
            active_cleared,
            write_snapshot_now,
            g_kbo_foreign_roster_audit_save_path);
    }
}

static void write_foreign_waiver_candidates(const char* source)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            append_log_line("foreign waiver scanner: no player vector");
        }
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

static void kbo_foreign_injury_replacement_scan_once(const char* source)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        return;
    }

    kbo_ensure_foreign_injury_replacements_loaded();

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    int scanned = 0;
    int opened = 0;
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
        scanned++;

        uint8_t injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int16_t days_left = *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        if (!injury_active || days_left < KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
        if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            if (league_id == 0u) {
                league_id = team_league_id;
            }
        }
        if (configured_league_id != 0u && league_id != 0u && league_id != configured_league_id) {
            continue;
        }

        KboForeignInjuryReplacement created_rec;
        memset(&created_rec, 0, sizeof(created_rec));
        int created = 0;
        kbo_lock_foreign_injury_replacements();
        int existing = kbo_find_foreign_injury_replacement_locked(player_id, 0);
        if (existing < 0 && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
            rec->team_id = team_id;
            rec->league_id = league_id != 0u ? league_id : configured_league_id;
            rec->injured_player_id = player_id;
            rec->replacement_player_id = 0u;
            rec->opened_on_yyyymmdd = today;
            rec->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)days_left);
            rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
            rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            rec->converted = 0u;
            created_rec = *rec;
            created = kbo_persist_foreign_injury_replacements_locked();
        }
        kbo_unlock_foreign_injury_replacements();

        if (created) {
            opened++;
            kbo_emit_foreign_injury_replacement_news(&created_rec, (int)days_left, "open");
            append_logf(
                "foreign injury replacement: opened source=%s team=%u player=%u league=%u days_left=%d slot=%s",
                source != NULL ? source : "",
                created_rec.team_id,
                created_rec.injured_player_id,
                created_rec.league_id,
                (int)days_left,
                kbo_foreign_injury_slot_label(created_rec.slot_type));
        }
    }

    KboForeignInjuryReplacement pending_news[16];
    int pending_count = 0;
    int changed = 0;
    memset(pending_news, 0, sizeof(pending_news));
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (!kbo_foreign_injury_status_uses_slot(rec->status)) {
            continue;
        }

        uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t injury_active = injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        if (injury_active && days_left > 0) {
            continue;
        }

        rec->status = KBO_FOREIGN_INJURY_STATUS_PENDING;
        if (pending_count < (int)(sizeof(pending_news) / sizeof(pending_news[0]))) {
            pending_news[pending_count++] = *rec;
        }
        changed = 1;
    }
    if (changed) {
        kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    for (int i = 0; i < pending_count; i++) {
        kbo_emit_foreign_injury_replacement_news(&pending_news[i], 0, "pending");
        append_logf(
            "foreign injury replacement: pending decision source=%s team=%u player=%u league=%u",
            source != NULL ? source : "",
            pending_news[i].team_id,
            pending_news[i].injured_player_id,
            pending_news[i].league_id);
    }

    if (opened > 0 || pending_count > 0) {
        append_logf(
            "foreign injury replacement: scan source=%s scanned_foreign=%d opened=%d pending=%d",
            source != NULL ? source : "",
            scanned,
            opened,
            pending_count);
    }
}

static DWORD WINAPI kbo_foreign_injury_replacement_thread(LPVOID parameter)
{
    (void)parameter;
    for (;;) {
        Sleep(7000);
        kbo_foreign_injury_replacement_scan_once("foreign_injury_replacement_thread");
    }
    return 0;
}

static void start_kbo_foreign_injury_replacement_thread(void)
{
    static LONG started = 0;
    if (!kbo_foreign_injury_replacement_enabled()) {
        append_log_line("foreign injury replacement: disabled");
        return;
    }
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_injury_replacement_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("foreign injury replacement thread started");
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("foreign injury replacement thread failed to start");
    }
}

static DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t tick = 0;
    for (;;) {
        Sleep(5000);
        tick++;
        process_foreign_waiver_commands();
        if (!kbo_is_foreign_waiver_negotiation_window_open()) {
            continue;
        }
        audit_foreign_roster_state("foreign_roster_pre_tick", 0);
        run_foreign_waiver_ai_core_once();
        if ((tick % 6u) == 0u) {
            audit_foreign_roster_state("foreign_roster_post_tick", 1);
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
