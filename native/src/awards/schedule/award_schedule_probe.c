#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "award_schedule_probe.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/names/team_string.h"

#define KBO_AWARD_PITCHER_NAME_OFFSET 0x150u
#define KBO_AWARD_MVP_NAME_OFFSET     0x168u
#define KBO_AWARD_ROOKIE_NAME_OFFSET  0x180u
#define KBO_AWARD_DEFENSE_NAME_OFFSET 0x198u
#define KBO_AWARD_FLAGS_BASE_OFFSET   0x385u
#define KBO_AWARD_FLAGS_COUNT         12u
#define KBO_AWARD_RULES_BASE_OFFSET   0x380u
#define KBO_AWARD_RULES_BYTES         0x80u
#define KBO_AWARD_HOF_RULES_OFFSET    0xc20u
#define KBO_AWARD_HOF_RULES_BYTES     0x30u
#define KBO_AWARD_SCHEDULE_POLICY_FILE "award_schedule_policy.json"
#define KBO_AWARD_SCHEDULE_MAX_RULES 16u
#define KBO_AWARD_SCHEDULE_MAX_TITLES 8u
#define KBO_AWARD_SCHEDULE_MAX_OVERRIDES 16u

static volatile LONG g_kbo_award_schedule_probe_started = 0;

typedef struct KboAwardScheduleOverride {
    uint32_t year;
    uint32_t month;
    uint32_t day;
} KboAwardScheduleOverride;

typedef struct KboAwardScheduleRule {
    char id[48];
    char titles[KBO_AWARD_SCHEDULE_MAX_TITLES][96];
    uint32_t title_count;
    uint32_t default_month;
    uint32_t default_day;
    KboAwardScheduleOverride overrides[KBO_AWARD_SCHEDULE_MAX_OVERRIDES];
    uint32_t override_count;
} KboAwardScheduleRule;

typedef struct KboAwardSchedulePolicy {
    KboAwardScheduleRule rules[KBO_AWARD_SCHEDULE_MAX_RULES];
    uint32_t rule_count;
} KboAwardSchedulePolicy;

static int kbo_award_schedule_read_file(const char* path, char** out_text, DWORD* out_size)
{
    if (path == NULL || out_text == NULL || out_size == NULL) {
        return 0;
    }
    *out_text = NULL;
    *out_size = 0u;

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }

    char* text = (char*)calloc((size_t)size.QuadPart + 1u, 1u);
    if (text == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    BOOL ok = ReadFile(file, text, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0u) {
        free(text);
        return 0;
    }
    text[read] = '\0';
    *out_text = text;
    *out_size = read;
    return 1;
}

static int kbo_award_schedule_load_text(char** out_text, DWORD* out_size, char* out_path, size_t out_path_size)
{
    char path[MAX_PATH] = {0};
    if (kbo_get_save_scoped_data_file(KBO_AWARD_SCHEDULE_POLICY_FILE, path, sizeof(path))
            && kbo_award_schedule_read_file(path, out_text, out_size)) {
        if (out_path != NULL && out_path_size > 0u) {
            snprintf(out_path, out_path_size, "%s", path);
        }
        return 1;
    }

    path[0] = '\0';
    if (kbo_get_global_data_file(KBO_AWARD_SCHEDULE_POLICY_FILE, path, sizeof(path))
            && kbo_award_schedule_read_file(path, out_text, out_size)) {
        if (out_path != NULL && out_path_size > 0u) {
            snprintf(out_path, out_path_size, "%s", path);
        }
        return 1;
    }
    return 0;
}

static const char* kbo_award_find_matching_brace(const char* open_brace)
{
    if (open_brace == NULL || *open_brace != '{') {
        return NULL;
    }
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    for (const char* p = open_brace; *p != '\0'; p++) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }
        if (ch == '"') {
            in_string = 1;
        } else if (ch == '{') {
            depth++;
        } else if (ch == '}') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static const char* kbo_award_find_matching_bracket(const char* open_bracket)
{
    if (open_bracket == NULL || *open_bracket != '[') {
        return NULL;
    }
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    for (const char* p = open_bracket; *p != '\0'; p++) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }
        if (ch == '"') {
            in_string = 1;
        } else if (ch == '[') {
            depth++;
        } else if (ch == ']') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static const char* kbo_award_json_key_in_range(const char* start, const char* end, const char* key)
{
    char pattern[80] = {0};
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t len = strlen(pattern);
    for (const char* p = start; p != NULL && p + len <= end; p++) {
        if (memcmp(p, pattern, len) == 0) {
            return p + len;
        }
    }
    return NULL;
}

static int kbo_award_json_uint_in_range(const char* start, const char* end, const char* key, uint32_t* out)
{
    const char* p = kbo_award_json_key_in_range(start, end, key);
    if (p == NULL) {
        return 0;
    }
    while (p < end && *p != ':') {
        p++;
    }
    if (p >= end) {
        return 0;
    }
    p++;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    if (p >= end || *p < '0' || *p > '9') {
        return 0;
    }
    uint32_t value = 0u;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        p++;
    }
    *out = value;
    return 1;
}

static int kbo_award_json_string_in_range(const char* start, const char* end, const char* key, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    const char* p = kbo_award_json_key_in_range(start, end, key);
    if (p == NULL) {
        return 0;
    }
    while (p < end && *p != ':') {
        p++;
    }
    if (p >= end) {
        return 0;
    }
    p++;
    while (p < end && *p != '"') {
        p++;
    }
    if (p >= end || *p != '"') {
        return 0;
    }
    p++;
    size_t used = 0u;
    while (p < end && *p != '"' && used + 1u < out_size) {
        if (*p == '\\' && p + 1 < end) {
            p++;
        }
        out[used++] = *p++;
    }
    out[used] = '\0';
    return used > 0u;
}

static void kbo_award_parse_titles(const char* start, const char* end, KboAwardScheduleRule* rule)
{
    const char* p = kbo_award_json_key_in_range(start, end, "match_titles");
    if (p == NULL) {
        return;
    }
    while (p < end && *p != '[') {
        p++;
    }
    const char* array_end = kbo_award_find_matching_bracket(p);
    if (array_end == NULL || array_end > end) {
        return;
    }
    p++;
    while (p < array_end && rule->title_count < KBO_AWARD_SCHEDULE_MAX_TITLES) {
        while (p < array_end && *p != '"') {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        p++;
        char* out = rule->titles[rule->title_count];
        size_t used = 0u;
        while (p < array_end && *p != '"' && used + 1u < 96u) {
            if (*p == '\\' && p + 1 < array_end) {
                p++;
            }
            out[used++] = *p++;
        }
        out[used] = '\0';
        if (used > 0u) {
            rule->title_count++;
        }
        if (p < array_end) {
            p++;
        }
    }
}

static void kbo_award_parse_overrides(const char* start, const char* end, KboAwardScheduleRule* rule)
{
    const char* p = kbo_award_json_key_in_range(start, end, "year_overrides");
    if (p == NULL) {
        return;
    }
    while (p < end && *p != '[') {
        p++;
    }
    const char* array_end = kbo_award_find_matching_bracket(p);
    if (array_end == NULL || array_end > end) {
        return;
    }
    p++;
    while (p < array_end && rule->override_count < KBO_AWARD_SCHEDULE_MAX_OVERRIDES) {
        while (p < array_end && *p != '{') {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        const char* object_end = kbo_award_find_matching_brace(p);
        if (object_end == NULL || object_end > array_end) {
            break;
        }
        KboAwardScheduleOverride item = {0};
        if (kbo_award_json_uint_in_range(p, object_end, "year", &item.year)
                && kbo_award_json_uint_in_range(p, object_end, "month", &item.month)
                && kbo_award_json_uint_in_range(p, object_end, "day", &item.day)
                && item.year >= 1982u && item.year <= 2400u
                && item.month >= 1u && item.month <= 12u
                && item.day >= 1u && item.day <= 31u) {
            rule->overrides[rule->override_count++] = item;
        }
        p = object_end + 1;
    }
}

static int kbo_award_schedule_parse_policy(const char* json, KboAwardSchedulePolicy* out)
{
    if (json == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    const char* awards = strstr(json, "\"awards\"");
    if (awards == NULL) {
        return 0;
    }
    const char* array = strchr(awards, '[');
    const char* array_end = kbo_award_find_matching_bracket(array);
    if (array == NULL || array_end == NULL) {
        return 0;
    }

    const char* p = array + 1;
    while (p < array_end && out->rule_count < KBO_AWARD_SCHEDULE_MAX_RULES) {
        while (p < array_end && *p != '{') {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        const char* object_end = kbo_award_find_matching_brace(p);
        if (object_end == NULL || object_end > array_end) {
            break;
        }

        KboAwardScheduleRule rule = {0};
        kbo_award_json_string_in_range(p, object_end, "id", rule.id, sizeof(rule.id));
        kbo_award_json_uint_in_range(p, object_end, "default_month", &rule.default_month);
        kbo_award_json_uint_in_range(p, object_end, "default_day", &rule.default_day);
        kbo_award_parse_titles(p, object_end, &rule);
        kbo_award_parse_overrides(p, object_end, &rule);
        if (rule.title_count > 0u
                && rule.default_month >= 1u && rule.default_month <= 12u
                && rule.default_day >= 1u && rule.default_day <= 31u) {
            out->rules[out->rule_count++] = rule;
        }
        p = object_end + 1;
    }

    return out->rule_count > 0u;
}

static uint32_t kbo_award_schedule_rule_date(const KboAwardScheduleRule* rule, uint32_t year)
{
    if (rule == NULL || year < 1982u || year > 2400u) {
        return 0u;
    }
    uint32_t month = rule->default_month;
    uint32_t day = rule->default_day;
    for (uint32_t i = 0u; i < rule->override_count; i++) {
        if (rule->overrides[i].year == year) {
            month = rule->overrides[i].month;
            day = rule->overrides[i].day;
            break;
        }
    }
    if (month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }
    return year * 10000u + month * 100u + day;
}

static int kbo_award_title_matches_rule(const KboAwardScheduleRule* rule, const char* title)
{
    if (rule == NULL || title == NULL || title[0] == '\0') {
        return 0;
    }
    for (uint32_t i = 0u; i < rule->title_count; i++) {
        if (_stricmp(rule->titles[i], title) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_schedule_apply_policy(uint32_t league_id, const KboAwardSchedulePolicy* policy, const char* path)
{
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

    int changed = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }
        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0
                || event[OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET] != 0
                || *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        char title[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title))) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t old_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t old_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        if (event_year < 1982u || event_year > 2400u) {
            continue;
        }

        for (uint32_t r = 0u; r < policy->rule_count; r++) {
            const KboAwardScheduleRule* rule = &policy->rules[r];
            if (!kbo_award_title_matches_rule(rule, title)) {
                continue;
            }

            uint32_t target = kbo_award_schedule_rule_date(rule, event_year);
            if (target == 0u) {
                continue;
            }
            uint32_t target_month = (target / 100u) % 100u;
            uint32_t target_day = target % 100u;
            if (old_month == target_month && old_day == target_day) {
                continue;
            }

            event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] = (uint8_t)target_month;
            event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] = (uint8_t)target_day;
            changed++;
            kbo_log_runtimef(
                "KBO award schedule event moved rule=%s title=%s league_id=%u event=%p date=%04u-%02u-%02u->%04u-%02u-%02u policy=%s",
                rule->id,
                title,
                league_id,
                (void*)event_ptr,
                event_year,
                old_month,
                old_day,
                event_year,
                target_month,
                target_day,
                path != NULL ? path : "");
            break;
        }
    }
    return changed;
}

static void kbo_award_schedule_apply_once(uint32_t league_id)
{
    char* json = NULL;
    DWORD json_size = 0u;
    char path[MAX_PATH] = {0};
    if (!kbo_award_schedule_load_text(&json, &json_size, path, sizeof(path))) {
        static volatile LONG s_missing_log_count = 0;
        if (InterlockedIncrement(&s_missing_log_count) <= 10) {
            kbo_log_runtimef("KBO award schedule policy unavailable file=%s", KBO_AWARD_SCHEDULE_POLICY_FILE);
        }
        return;
    }

    KboAwardSchedulePolicy policy;
    if (kbo_award_schedule_parse_policy(json, &policy)) {
        int changed = kbo_award_schedule_apply_policy(league_id, &policy, path);
        if (changed > 0) {
            kbo_log_runtimef("KBO award schedule policy applied changed=%d rules=%u path=%s", changed, policy.rule_count, path);
        }
    } else {
        kbo_log_runtimef("KBO award schedule policy parse failed path=%s size=%lu", path, (unsigned long)json_size);
    }
    free(json);
}

static void kbo_award_probe_copy_name(
    uint8_t* league,
    uint32_t offset,
    const char* label,
    char* out,
    size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (league == NULL || label == NULL || out == NULL || out_size == 0u) {
        return;
    }

    if (!copy_ootp_string_object_text(league, offset, out, out_size)) {
        snprintf(out, out_size, "<unreadable>");
    }
}

static void kbo_log_award_schedule_probe(uintptr_t league_ptr, uint32_t league_id, uint32_t date_key)
{
    uint8_t* league = (uint8_t*)league_ptr;
    if (league == NULL
            || !memory_range_readable(league, KBO_AWARD_RULES_BASE_OFFSET + KBO_AWARD_RULES_BYTES)
            || !memory_range_readable(league + KBO_AWARD_HOF_RULES_OFFSET, KBO_AWARD_HOF_RULES_BYTES)) {
        kbo_log_runtimef(
            "KBO award schedule probe skipped league=%p league_id=%u reason=league_unreadable",
            (void*)league_ptr,
            league_id);
        return;
    }

    char pitcher[96] = {0};
    char mvp[96] = {0};
    char rookie[96] = {0};
    char defense[96] = {0};
    kbo_award_probe_copy_name(league, KBO_AWARD_PITCHER_NAME_OFFSET, "pitcher", pitcher, sizeof(pitcher));
    kbo_award_probe_copy_name(league, KBO_AWARD_MVP_NAME_OFFSET, "mvp", mvp, sizeof(mvp));
    kbo_award_probe_copy_name(league, KBO_AWARD_ROOKIE_NAME_OFFSET, "rookie", rookie, sizeof(rookie));
    kbo_award_probe_copy_name(league, KBO_AWARD_DEFENSE_NAME_OFFSET, "defense", defense, sizeof(defense));

    char flags[192] = {0};
    size_t used = 0u;
    for (uint32_t i = 0u; i < KBO_AWARD_FLAGS_COUNT; i++) {
        uint32_t offset = KBO_AWARD_FLAGS_BASE_OFFSET + i;
        int wrote = snprintf(
            flags + used,
            used < sizeof(flags) ? sizeof(flags) - used : 0u,
            "%s%03x=%u",
            i == 0u ? "" : " ",
            offset,
            (unsigned)league[offset]);
        if (wrote <= 0) {
            break;
        }
        used += (size_t)wrote;
        if (used >= sizeof(flags)) {
            break;
        }
    }

    kbo_log_runtimef(
        "KBO award schedule probe league=%p league_id=%u date=%u names={pitcher:'%s',mvp:'%s',rookie:'%s',defense:'%s'} flags=%s",
        (void*)league_ptr,
        league_id,
        date_key,
        pitcher,
        mvp,
        rookie,
        defense,
        flags);

    kbo_log_runtimef(
        "KBO award schedule probe bytes league=%p rules380=%08x/%08x/%08x/%08x/%08x/%08x/%08x/%08x hofc20=%08x/%08x/%08x/%08x",
        (void*)league_ptr,
        *(uint32_t*)(league + 0x380u),
        *(uint32_t*)(league + 0x384u),
        *(uint32_t*)(league + 0x388u),
        *(uint32_t*)(league + 0x38cu),
        *(uint32_t*)(league + 0x390u),
        *(uint32_t*)(league + 0x394u),
        *(uint32_t*)(league + 0x398u),
        *(uint32_t*)(league + 0x39cu),
        *(uint32_t*)(league + 0xc20u),
        *(uint32_t*)(league + 0xc24u),
        *(uint32_t*)(league + 0xc28u),
        *(uint32_t*)(league + 0xc2cu));
}

static DWORD WINAPI kbo_award_schedule_probe_thread(LPVOID parameter)
{
    (void)parameter;

    uintptr_t last_league = 0u;
    uint32_t last_date = 0u;
    int logged = 0;

    kbo_log_runtime_line("KBO award schedule probe started");

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(2000u)) {
            break;
        }
        if (!kbo_fix_enabled()) {
            continue;
        }

        uint32_t year = 0u;
        uint32_t month = 0u;
        uint32_t day = 0u;
        if (!kbo_current_date_is_valid(&year, &month, &day)) {
            continue;
        }
        uint32_t date_key = year * 10000u + month * 100u + day;

        uint32_t league_id = kbo_resolve_kbo_league_id();
        uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
        if (league_ptr == 0u) {
            continue;
        }

        if (!logged || league_ptr != last_league || date_key != last_date) {
            kbo_log_award_schedule_probe(league_ptr, league_id, date_key);
            logged = 1;
            last_league = league_ptr;
            last_date = date_key;
        }
        kbo_award_schedule_apply_once(league_id);
    }

    kbo_log_runtime_line("KBO award schedule probe stopped");
    InterlockedExchange(&g_kbo_award_schedule_probe_started, 0);
    return 0;
}

int start_kbo_award_schedule_probe_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_award_schedule_probe_started, 1, 0) != 0) {
        return 1;
    }
    if (!kbo_start_runtime_thread(kbo_award_schedule_probe_thread, NULL, "award schedule probe")) {
        InterlockedExchange(&g_kbo_award_schedule_probe_started, 0);
        return 0;
    }
    return 1;
}
