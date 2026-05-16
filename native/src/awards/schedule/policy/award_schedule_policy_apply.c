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

static const char* kbo_award_schedule_rule_title_for_language(const KboAwardScheduleRule* rule)
{
    if (rule == NULL) {
        return "";
    }
    if (kbo_get_custom_news_language_setting() == KBO_CUSTOM_NEWS_LANGUAGE_KO
            && rule->label_ko[0] != '\0') {
        return rule->label_ko;
    }
    if (kbo_get_custom_news_language_setting() == KBO_CUSTOM_NEWS_LANGUAGE_EN
            && rule->label_en[0] != '\0') {
        return rule->label_en;
    }
    if (rule->label[0] != '\0') {
        return rule->label;
    }
    if (rule->title_count > 0u && rule->titles[0][0] != '\0') {
        return rule->titles[0];
    }
    return rule->id;
}

static int kbo_award_schedule_event_is_placeholder_custom(
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

static void kbo_award_schedule_mark_custom_placeholder_over(uint8_t* event)
{
    if (event == NULL
            || !memory_range_readable(event, OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET + sizeof(uint16_t))) {
        return;
    }
    uint16_t* event_over = (uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
    DWORD old_protect = 0;
    if (VirtualProtect(event_over, sizeof(*event_over), PAGE_READWRITE, &old_protect)) {
        *event_over = 1u;
        DWORD ignored = 0;
        VirtualProtect(event_over, sizeof(*event_over), old_protect, &ignored);
    }
}

static int kbo_award_schedule_assign_event_title(uint8_t* event, const char* title)
{
    if (event == NULL || title == NULL || title[0] == '\0') {
        return 0;
    }

    char* internal_title = kbo_alloc_ootp_internal_text(title);
    const char* title_for_ootp = internal_title != NULL ? internal_title : title;

    char current[160] = {0};
    if (copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, current, sizeof(current))
            && strcmp(current, title_for_ootp) == 0) {
        kbo_free_ootp_internal_text(internal_title);
        return 0;
    }

    int assigned = assign_ootp_string_object_text(
        event,
        OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET,
        title_for_ootp);
    kbo_free_ootp_internal_text(internal_title);
    return assigned;
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
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET) != 0
                || *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        char title[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title))) {
            continue;
        }

        uint32_t event_type = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
        if (kbo_award_schedule_event_is_placeholder_custom(event_type, title, policy)) {
            int deleted = kbo_award_schedule_set_deleted(event, 1u);
            kbo_award_schedule_mark_custom_placeholder_over(event);
            if (deleted) {
                changed++;
                kbo_log_runtimef(
                    "KBO award schedule placeholder custom event deleted title=%s league_id=%u event=%p date=%04u-%02u-%02u policy=%s",
                    title,
                    league_id,
                    (void*)event_ptr,
                    (uint32_t)*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET),
                    (uint32_t)event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET],
                    (uint32_t)event[OOTP27_LEAGUE_EVENT_DAY_OFFSET],
                    path != NULL ? path : "");
            }
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

            const char* desired_title = kbo_award_schedule_rule_title_for_language(rule);
            if (desired_title[0] != '\0' && kbo_award_schedule_assign_event_title(event, desired_title)) {
                changed++;
                kbo_log_runtimef(
                    "KBO award schedule native event title localized rule=%s old_title=%s new_title=%s type=%u league_id=%u event=%p",
                    rule->id,
                    title,
                    desired_title,
                    event_type,
                    league_id,
                    (void*)event_ptr);
            }

            if (kbo_award_schedule_set_deleted(event, 0u)) {
                changed++;
                kbo_log_runtimef(
                    "KBO award schedule native event enabled rule=%s title=%s type=%u league_id=%u event=%p current_date=%08u",
                    rule->id,
                    desired_title[0] != '\0' ? desired_title : title,
                    event_type,
                    league_id,
                    (void*)event_ptr,
                    current_date);
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
