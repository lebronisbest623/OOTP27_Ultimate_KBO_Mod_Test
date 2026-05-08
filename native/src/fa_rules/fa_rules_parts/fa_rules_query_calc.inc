static int kbo_fa_rules_case_is_compensable(const KboFaRules* rules, const char* case_label)
{
    if (rules == NULL || case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    for (int i = 0; i < rules->compensable_case_count; i++) {
        if (strcmp(rules->compensable_cases[i], case_label) == 0) {
            return 1;
        }
    }
    return 0;
}

static const KboFaRuleGradeConfig* kbo_fa_rules_find_grade(const KboFaRules* rules, const char* grade)
{
    if (rules == NULL || grade == NULL || grade[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < rules->grade_count; i++) {
        if (rules->grades[i].enabled && _stricmp(rules->grades[i].grade, grade) == 0) {
            return &rules->grades[i];
        }
    }
    return NULL;
}

static int kbo_fa_rules_grade_is_compensable(const KboFaRules* rules, const char* grade)
{
    const KboFaRuleGradeConfig* rule = kbo_fa_rules_find_grade(rules, grade);
    return rule != NULL && rule->cash_only_percent > 0u;
}

static void kbo_fa_rules_calculate_compensation(
    const KboFaRules* rules,
    const char* grade,
    int32_t previous_salary,
    uint32_t* out_cash_with_player,
    uint32_t* out_cash_only,
    uint32_t* out_protect_count,
    uint8_t* out_requires_player)
{
    if (out_cash_with_player != NULL) { *out_cash_with_player = 0u; }
    if (out_cash_only != NULL) { *out_cash_only = 0u; }
    if (out_protect_count != NULL) { *out_protect_count = 0u; }
    if (out_requires_player != NULL) { *out_requires_player = 0u; }
    if (previous_salary <= 0) {
        return;
    }

    const KboFaRuleGradeConfig* rule = kbo_fa_rules_find_grade(rules, grade);
    if (rule == NULL) {
        return;
    }

    uint64_t salary = (uint64_t)(uint32_t)previous_salary;
    uint64_t amount = 0u;
    if (rule->cash_with_player_percent > 0u) {
        amount = (salary * (uint64_t)rule->cash_with_player_percent) / 100u;
        if (out_cash_with_player != NULL) {
            *out_cash_with_player = amount > UINT32_MAX ? UINT32_MAX : (uint32_t)amount;
        }
    }
    if (rule->cash_only_percent > 0u) {
        amount = (salary * (uint64_t)rule->cash_only_percent) / 100u;
        if (out_cash_only != NULL) {
            *out_cash_only = amount > UINT32_MAX ? UINT32_MAX : (uint32_t)amount;
        }
    }
    if (out_protect_count != NULL) {
        *out_protect_count = rule->protect_count;
    }
    if (out_requires_player != NULL) {
        *out_requires_player = rule->requires_player_compensation;
    }
}
