#include "../award_schedule_probe_internal.h"

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

static const char* kbo_award_schedule_rule_title(const KboAwardScheduleRule* rule)
{
    if (rule == NULL) {
        return "";
    }
    if (rule->label[0] != '\0') {
        return rule->label;
    }
    if (rule->title_count > 0u && rule->titles[0][0] != '\0') {
        return rule->titles[0];
    }
    return rule->id;
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

static int kbo_award_schedule_ensure_policy_events(
    uint32_t league_id,
    const KboAwardSchedulePolicy* policy,
    uint32_t current_date,
    const char* path)
{
    if (league_id == 0u || policy == NULL || current_date < 19820101u) {
        return 0;
    }

    uint32_t current_year = current_date / 10000u;
    int created = 0;
    int ensured = 0;
    for (uint32_t i = 0u; i < policy->rule_count; i++) {
        const KboAwardScheduleRule* rule = &policy->rules[i];
        uint32_t target = kbo_award_schedule_rule_date(rule, current_year);
        if (target == 0u || target < current_date) {
            continue;
        }

        const char* title = kbo_award_schedule_rule_title(rule);
        uint32_t month = (target / 100u) % 100u;
        uint32_t day = target % 100u;
        int did_create = create_kbo_league_event(
            current_year,
            month,
            day,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            title,
            0u,
            "award_schedule_policy");
        if (did_create) {
            created++;
        }
        ensured++;
        kbo_log_runtimef(
            "KBO award schedule policy event ensured rule=%s title=%s date=%04u-%02u-%02u created=%d league_id=%u path=%s",
            rule->id,
            title,
            current_year,
            month,
            day,
            did_create,
            league_id,
            path != NULL ? path : "");
    }
    return created > 0 ? created : ensured;
}

void kbo_award_schedule_apply_once(uint32_t league_id, uint32_t current_date, int ensure_events)
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
        int ensured = 0;
        if (ensure_events) {
            ensured = kbo_award_schedule_ensure_policy_events(league_id, &policy, current_date, path);
        }
        if (changed > 0) {
            kbo_log_runtimef("KBO award schedule policy applied changed=%d rules=%u path=%s", changed, policy.rule_count, path);
        }
        if (ensured > 0) {
            kbo_log_runtimef(
                "KBO award schedule policy ensured events=%d rules=%u current_date=%08u path=%s",
                ensured,
                policy.rule_count,
                current_date,
                path);
        }
    } else {
        kbo_log_runtimef("KBO award schedule policy parse failed path=%s size=%lu", path, (unsigned long)json_size);
    }
    free(json);
}
