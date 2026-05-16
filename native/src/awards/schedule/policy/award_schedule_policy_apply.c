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
    if ((rule->label[0] != '\0' && _stricmp(rule->label, title) == 0)
            || (rule->label_en[0] != '\0' && _stricmp(rule->label_en, title) == 0)
            || (rule->label_ko[0] != '\0' && strcmp(rule->label_ko, title) == 0)) {
        return 1;
    }
    for (uint32_t i = 0u; i < rule->title_count; i++) {
        if (_stricmp(rule->titles[i], title) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_event_type_matches_rule(const KboAwardScheduleRule* rule, uint32_t event_type)
{
    if (rule == NULL) {
        return 0;
    }
    for (uint32_t i = 0u; i < rule->event_type_count; i++) {
        if ((uint32_t)rule->event_types[i] == event_type) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_event_matches_rule(const KboAwardScheduleRule* rule, uint32_t event_type, const char* title)
{
    return kbo_award_event_type_matches_rule(rule, event_type)
        || kbo_award_title_matches_rule(rule, title);
}

static int kbo_award_schedule_event_is_legacy_custom_placeholder(
    uint32_t event_type,
    const char* title,
    const KboAwardSchedulePolicy* policy)
{
    if (policy == NULL || event_type != OOTP27_EVENT_TYPE_CUSTOM_EVENT || title == NULL || title[0] == '\0') {
        return 0;
    }
    for (uint32_t r = 0u; r < policy->rule_count; r++) {
        const KboAwardScheduleRule* rule = &policy->rules[r];
        if ((rule->label[0] != '\0' && _stricmp(rule->label, title) == 0)
                || (rule->label_en[0] != '\0' && _stricmp(rule->label_en, title) == 0)
                || (rule->label_ko[0] != '\0' && strcmp(rule->label_ko, title) == 0)) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_schedule_write_u8(uint8_t* slot, uint8_t value)
{
    if (slot == NULL || !memory_range_readable(slot, sizeof(*slot))) {
        return 0;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
        return 0;
    }
    *slot = value;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), old_protect, &ignored);
    return 1;
}

static int kbo_award_schedule_write_u16(uint8_t* slot, uint16_t value)
{
    if (slot == NULL || !memory_range_readable(slot, sizeof(value))) {
        return 0;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(value), PAGE_READWRITE, &old_protect)) {
        return 0;
    }
    *(uint16_t*)slot = value;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(value), old_protect, &ignored);
    return 1;
}

static int kbo_award_schedule_write_date(uint8_t* event, uint32_t month, uint32_t day)
{
    if (event == NULL || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }
    int wrote_month = kbo_award_schedule_write_u8(
        event + OOTP27_LEAGUE_EVENT_MONTH_OFFSET,
        (uint8_t)month);
    int wrote_day = kbo_award_schedule_write_u8(
        event + OOTP27_LEAGUE_EVENT_DAY_OFFSET,
        (uint8_t)day);
    return wrote_month && wrote_day;
}

static int kbo_award_schedule_write_year(uint8_t* event, uint32_t year)
{
    if (event == NULL || year < 1982u || year > 2400u) {
        return 0;
    }
    return kbo_award_schedule_write_u16(
        event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET,
        (uint16_t)year);
}

static int kbo_award_schedule_set_deleted(uint8_t* event, uint8_t deleted)
{
    if (event == NULL) {
        return 0;
    }
    if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] == deleted) {
        return 0;
    }
    return kbo_award_schedule_write_u8(event + OOTP27_LEAGUE_EVENT_DELETED_OFFSET, deleted);
}

static int kbo_award_schedule_set_event_over(uint8_t* event, uint16_t value)
{
    if (event == NULL
            || !memory_range_readable(event, OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET + sizeof(uint16_t))) {
        return 0;
    }
    if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET) == value) {
        return 0;
    }
    return kbo_award_schedule_write_u16(
        event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET,
        value);
}

static uint32_t kbo_award_schedule_retired_placeholder_year(uint32_t current_date)
{
    uint32_t current_year = current_date / 10000u;
    return current_year > 1982u && current_year <= 2400u ? current_year - 1u : 1982u;
}

static int kbo_award_schedule_retire_legacy_custom_placeholder(uint8_t* event, uint32_t current_date)
{
    if (event == NULL) {
        return 0;
    }
    uint32_t target_year = kbo_award_schedule_retired_placeholder_year(current_date);
    uint32_t old_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
    uint32_t old_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
    uint32_t old_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];

    int changed = 0;
    if (old_year != target_year) {
        changed |= kbo_award_schedule_write_year(event, target_year);
    }
    if (old_month != 1u || old_day != 1u) {
        changed |= kbo_award_schedule_write_date(event, 1u, 1u);
    }
    changed |= kbo_award_schedule_set_event_over(event, 1u);
    /* event_over spans the deleted byte in this layout, so deleted must be restored last. */
    changed |= kbo_award_schedule_set_deleted(event, 1u);
    return changed;
}

static int kbo_award_schedule_apply_policy(
    uint32_t league_id,
    uint32_t current_date,
    const KboAwardSchedulePolicy* policy,
    const char* path)
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
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        char title[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title))) {
            continue;
        }

        uint32_t event_type = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
        if (event_type == OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            if (kbo_award_schedule_event_is_legacy_custom_placeholder(event_type, title, policy)) {
                uint32_t old_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
                uint32_t old_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
                uint32_t old_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
                int retired = kbo_award_schedule_retire_legacy_custom_placeholder(event, current_date);
                if (retired) {
                    changed++;
                    kbo_log_runtimef(
                        "KBO award schedule legacy custom event retired title=%s league_id=%u event=%p date=%04u-%02u-%02u->%04u-01-01 policy=%s",
                        title,
                        league_id,
                        (void*)event_ptr,
                        old_year,
                        old_month,
                        old_day,
                        kbo_award_schedule_retired_placeholder_year(current_date),
                        path != NULL ? path : "");
                }
            }
            continue;
        }

        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET) != 0) {
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
            if (!kbo_award_event_matches_rule(rule, event_type, title)) {
                continue;
            }

            uint32_t target = kbo_award_schedule_rule_date(rule, event_year);
            if (target == 0u) {
                continue;
            }
            uint32_t target_month = (target / 100u) % 100u;
            uint32_t target_day = target % 100u;
            if (old_month != target_month || old_day != target_day) {
                if (kbo_award_schedule_write_date(event, target_month, target_day)) {
                    changed++;
                    kbo_log_runtimef(
                        "KBO award schedule native event moved rule=%s title=%s type=%u league_id=%u event=%p date=%04u-%02u-%02u->%04u-%02u-%02u policy=%s",
                        rule->id,
                        title,
                        event_type,
                        league_id,
                        (void*)event_ptr,
                        event_year,
                        old_month,
                        old_day,
                        event_year,
                        target_month,
                        target_day,
                        path != NULL ? path : "");
                }
            }

            break;
        }
    }
    return changed;
}

void kbo_award_schedule_apply_once(uint32_t league_id, uint32_t current_date, int ensure_events)
{
    (void)ensure_events;
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
        int changed = kbo_award_schedule_apply_policy(league_id, current_date, &policy, path);
        if (changed > 0) {
            kbo_log_runtimef("KBO award schedule policy applied changed=%d rules=%u path=%s", changed, policy.rule_count, path);
        }
    } else {
        kbo_log_runtimef("KBO award schedule policy parse failed path=%s size=%lu", path, (unsigned long)json_size);
    }
    free(json);
}
