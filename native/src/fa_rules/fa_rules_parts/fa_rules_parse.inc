static void kbo_fa_rules_parse_cases(KboFaRules* rules, const char* start, const char* end)
{
    if (rules == NULL || start == NULL || end == NULL) {
        return;
    }
    start = kbo_json_skip_ws(start, end);
    if (start >= end || *start != '[') {
        return;
    }

    char parsed[KBO_FA_RULES_MAX_CASES][48];
    memset(parsed, 0, sizeof(parsed));
    int count = 0;
    const char* p = start + 1;
    while (p < end && count < KBO_FA_RULES_MAX_CASES) {
        p = kbo_json_skip_ws(p, end);
        if (p >= end || *p == ']') {
            break;
        }
        if (*p == '"') {
            const char* stop = kbo_json_find_string_end(p, end);
            if (stop == NULL) {
                break;
            }
            if (kbo_fa_rules_copy_json_string_value(p, stop + 1, parsed[count], sizeof(parsed[count]))) {
                count++;
            }
            p = stop + 1;
        } else {
            p = kbo_fa_rules_json_value_end(p, end);
        }
        p = kbo_json_skip_ws(p, end);
        if (p < end && *p == ',') {
            p++;
        }
    }

    if (count > 0) {
        memset(rules->compensable_cases, 0, sizeof(rules->compensable_cases));
        rules->compensable_case_count = 0;
        for (int i = 0; i < count; i++) {
            kbo_fa_rules_add_case(rules, parsed[i]);
        }
    }
}

static void kbo_fa_rules_parse_grade_object(
    KboFaRules* rules,
    const char* grade,
    const char* start,
    const char* end)
{
    KboFaRuleGradeConfig* rule = kbo_fa_rules_ensure_grade(rules, grade);
    if (rule == NULL || start == NULL || end == NULL) {
        return;
    }

    uint32_t u32_value = 0u;
    uint8_t bool_value = 0u;
    if (kbo_fa_rules_read_u32(start, end, "protectedCount", &u32_value)
            || kbo_fa_rules_read_u32(start, end, "protectCount", &u32_value)) {
        rule->protect_count = u32_value;
    }
    if (kbo_fa_rules_read_u32(start, end, "cashWithPlayerPercent", &u32_value)
            || kbo_fa_rules_read_u32(start, end, "cashPlusPlayerPercent", &u32_value)) {
        rule->cash_with_player_percent = u32_value;
    }
    if (kbo_fa_rules_read_u32(start, end, "cashOnlyPercent", &u32_value)) {
        rule->cash_only_percent = u32_value;
    }
    if (kbo_fa_rules_read_bool(start, end, "requiresPlayerCompensation", &bool_value)) {
        rule->requires_player_compensation = bool_value;
    } else {
        rule->requires_player_compensation =
            (rule->protect_count > 0u && rule->cash_with_player_percent > 0u) ? 1u : 0u;
    }
    if (kbo_fa_rules_read_bool(start, end, "enabled", &bool_value)) {
        rule->enabled = bool_value;
    }
}

static void kbo_fa_rules_parse_json(KboFaRules* rules, const char* json, DWORD json_size)
{
    if (rules == NULL || json == NULL || json_size == 0u) {
        return;
    }
    const char* root_start = json;
    const char* root_end = json + json_size;
    const char* fa_start = root_start;
    const char* fa_end = root_end;
    const char* value_start = NULL;
    const char* value_end = NULL;

    uint32_t u32_value = 0u;
    uint8_t bool_value = 0u;
    if (kbo_fa_rules_read_u32(root_start, root_end, "version", &u32_value)) {
        rules->version = u32_value;
    }
    if (kbo_fa_rules_find_json_value_span(root_start, root_end, "faCompensation", &value_start, &value_end)) {
        fa_start = value_start;
        fa_end = value_end;
    }

    if (kbo_fa_rules_read_bool(fa_start, fa_end, "excludeForeignPlayers", &bool_value)) {
        rules->exclude_foreign_players = bool_value;
    }
    if (kbo_fa_rules_read_u32(fa_start, fa_end, "protectedListDueDays", &u32_value)
            && u32_value <= 30u) {
        rules->protected_list_due_days = u32_value;
    }

    if (kbo_fa_rules_find_json_value_span(fa_start, fa_end, "ageGradeOverride", &value_start, &value_end)) {
        if (kbo_fa_rules_read_bool(value_start, value_end, "enabled", &bool_value)) {
            rules->age_grade_override_enabled = bool_value;
        }
        if (kbo_fa_rules_read_u32(value_start, value_end, "minAge", &u32_value)) {
            rules->age_grade_min_age = u32_value;
        }
        kbo_fa_rules_read_string(value_start, value_end, "grade", rules->age_grade, sizeof(rules->age_grade));
    }

    if (kbo_fa_rules_find_json_value_span(fa_start, fa_end, "compensableCases", &value_start, &value_end)) {
        kbo_fa_rules_parse_cases(rules, value_start, value_end);
    }

    if (kbo_fa_rules_find_json_value_span(fa_start, fa_end, "grades", &value_start, &value_end)) {
        const char* grade_start = NULL;
        const char* grade_end = NULL;
        if (kbo_fa_rules_find_json_value_span(value_start, value_end, "A", &grade_start, &grade_end)) {
            kbo_fa_rules_parse_grade_object(rules, "A", grade_start, grade_end);
        }
        if (kbo_fa_rules_find_json_value_span(value_start, value_end, "B", &grade_start, &grade_end)) {
            kbo_fa_rules_parse_grade_object(rules, "B", grade_start, grade_end);
        }
        if (kbo_fa_rules_find_json_value_span(value_start, value_end, "C", &grade_start, &grade_end)) {
            kbo_fa_rules_parse_grade_object(rules, "C", grade_start, grade_end);
        }
    }
}
