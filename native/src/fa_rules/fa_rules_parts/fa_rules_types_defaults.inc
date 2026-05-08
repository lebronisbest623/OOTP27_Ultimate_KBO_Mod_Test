/* Externalized KBO FA rule configuration. */

#define KBO_FA_RULES_MAX_GRADES 8
#define KBO_FA_RULES_MAX_CASES 16

typedef struct KboFaRuleGradeConfig {
    char grade[12];
    uint32_t protect_count;
    uint32_t cash_with_player_percent;
    uint32_t cash_only_percent;
    uint8_t requires_player_compensation;
    uint8_t enabled;
} KboFaRuleGradeConfig;

typedef struct KboFaRules {
    uint32_t version;
    uint8_t exclude_foreign_players;
    uint32_t protected_list_due_days;
    uint8_t age_grade_override_enabled;
    uint32_t age_grade_min_age;
    char age_grade[12];
    int grade_count;
    KboFaRuleGradeConfig grades[KBO_FA_RULES_MAX_GRADES];
    int compensable_case_count;
    char compensable_cases[KBO_FA_RULES_MAX_CASES][48];
    char source_path[MAX_PATH];
    uint8_t loaded_from_file;
} KboFaRules;

static void kbo_fa_rules_copy_text(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }
    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    if (len > 0u) {
        memcpy(out, value, len);
    }
    out[len] = '\0';
}

static KboFaRuleGradeConfig* kbo_fa_rules_ensure_grade(KboFaRules* rules, const char* grade)
{
    if (rules == NULL || grade == NULL || grade[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < rules->grade_count; i++) {
        if (_stricmp(rules->grades[i].grade, grade) == 0) {
            return &rules->grades[i];
        }
    }
    if (rules->grade_count >= KBO_FA_RULES_MAX_GRADES) {
        return NULL;
    }
    KboFaRuleGradeConfig* rule = &rules->grades[rules->grade_count++];
    memset(rule, 0, sizeof(*rule));
    kbo_fa_rules_copy_text(grade, rule->grade, sizeof(rule->grade));
    rule->enabled = 1u;
    return rule;
}

static void kbo_fa_rules_add_case(KboFaRules* rules, const char* case_label)
{
    if (rules == NULL || case_label == NULL || case_label[0] == '\0'
            || rules->compensable_case_count >= KBO_FA_RULES_MAX_CASES) {
        return;
    }
    for (int i = 0; i < rules->compensable_case_count; i++) {
        if (strcmp(rules->compensable_cases[i], case_label) == 0) {
            return;
        }
    }
    kbo_fa_rules_copy_text(
        case_label,
        rules->compensable_cases[rules->compensable_case_count],
        sizeof(rules->compensable_cases[0]));
    rules->compensable_case_count++;
}

static void kbo_fa_rules_init_defaults(KboFaRules* rules)
{
    if (rules == NULL) {
        return;
    }
    memset(rules, 0, sizeof(*rules));
    rules->version = 1u;
    rules->exclude_foreign_players = 1u;
    rules->protected_list_due_days = 3u;
    rules->age_grade_override_enabled = 1u;
    rules->age_grade_min_age = 35u;
    kbo_fa_rules_copy_text("C", rules->age_grade, sizeof(rules->age_grade));
    kbo_fa_rules_copy_text("built-in KBO defaults", rules->source_path, sizeof(rules->source_path));

    KboFaRuleGradeConfig* grade_a = kbo_fa_rules_ensure_grade(rules, "A");
    if (grade_a != NULL) {
        grade_a->protect_count = 20u;
        grade_a->cash_with_player_percent = 200u;
        grade_a->cash_only_percent = 300u;
        grade_a->requires_player_compensation = 1u;
    }
    KboFaRuleGradeConfig* grade_b = kbo_fa_rules_ensure_grade(rules, "B");
    if (grade_b != NULL) {
        grade_b->protect_count = 25u;
        grade_b->cash_with_player_percent = 100u;
        grade_b->cash_only_percent = 200u;
        grade_b->requires_player_compensation = 1u;
    }
    KboFaRuleGradeConfig* grade_c = kbo_fa_rules_ensure_grade(rules, "C");
    if (grade_c != NULL) {
        grade_c->cash_only_percent = 150u;
    }

    kbo_fa_rules_add_case(rules, "KBO_FA_APPROVED");
    kbo_fa_rules_add_case(rules, "KBO_FA_ELIGIBLE_NOT_APPROVED");
    kbo_fa_rules_add_case(rules, "KBO_FA_DEFERRED");
    kbo_fa_rules_add_case(rules, "KBO_FA_BY_HISTORY_UNGRADED");
}
