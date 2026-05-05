#include <stdint.h>

/* ---- Foreign waiver intelligence scaffold ---- */

#define KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT 3u

#include "foreign/foreign_waiver_decls.inc"
#include "foreign/foreign_waiver_config.inc"
#include "foreign/foreign_waiver_state.inc"
#include "foreign/foreign_waiver_policy.inc"
#include "foreign/foreign_waiver_events.inc"
#include "foreign/foreign_waiver_player_eval.inc"

#include "foreign/foreign_replacement_seed.inc"

#include "foreign/foreign_injury_replacement.inc"

#include "foreign/foreign_asian_quota.inc"

#include "foreign/foreign_waiver_rights.inc"

#include "foreign/foreign_waiver_results.inc"

#include "foreign/foreign_waiver_window.inc"

#include "foreign/foreign_signability_hooks.inc"

#include "foreign/foreign_waiver_status.inc"
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

    /*
     * OOTP saves can carry an active_team_id for foreign players at league start.
     * That is only the club with priority to decide during the KBO reserve-rights
     * event, not an already exercised reserve right. Stored rights are handled by
     * foreign_waiver_rights.csv; candidate ownership should appear only while the
     * negotiation window is open.
     */
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
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

#include "foreign/foreign_waiver_ai.inc"

#include "foreign/foreign_waiver_io.inc"

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

#include "foreign/foreign_roster_audit.inc"

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
    if (!read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt")) {
        append_log_line("foreign waiver scanner thread disabled: enable_foreign_waiver_background_scanner.txt is off");
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
