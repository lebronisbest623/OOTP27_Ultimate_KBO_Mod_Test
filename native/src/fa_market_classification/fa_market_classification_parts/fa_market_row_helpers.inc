static void kbo_fa_market_set_history_reason(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    const char* prefix)
{
    if (row == NULL) {
        return;
    }
    if (history != NULL && history->history_date[0] != '\0' && history->history_text[0] != '\0') {
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s history=%s %.96s",
            prefix != NULL ? prefix : "player history match",
            history->history_date,
            history->history_text);
        return;
    }
    snprintf(row->reason, sizeof(row->reason), "%s", prefix != NULL ? prefix : "player history match");
}

static int kbo_fa_market_apply_history_case(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history)
{
    if (row == NULL || history == NULL || !history->found) {
        return 0;
    }

    if (history->undrafted_free_agent) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        kbo_fa_market_set_history_reason(row, history, "player history says undrafted free agent");
        return 1;
    }

    if (history->released) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_RELEASED_NON_FA");
        kbo_fa_market_set_history_reason(row, history, "player history says released, not official FA");
        return 1;
    }

    if (history->became_free_agent) {
        if (kbo_fa_market_row_is_independent_league_fa(row)) {
            snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_LEAGUE_FA");
            kbo_fa_market_set_history_reason(row, history, "player history says free agent from independent-league context");
            return 1;
        }

        snprintf(row->case_label, sizeof(row->case_label), "KBO_FA_BY_HISTORY_UNGRADED");
        kbo_fa_market_set_history_reason(row, history, "player history says became a free agent; grade pending salary snapshot or seed");
        return 1;
    }

    return 0;
}

static int kbo_fa_market_grade_is_unknown(const char* grade)
{
    return grade == NULL
        || grade[0] == '\0'
        || _stricmp(grade, "UNKNOWN") == 0
        || strcmp(grade, "-") == 0;
}

static const char* kbo_fa_market_display_grade(const char* grade)
{
    return kbo_fa_market_grade_is_unknown(grade) ? "-" : grade;
}

static uint32_t kbo_fa_market_display_grade_sort_rank(const char* grade)
{
    if (grade != NULL && _stricmp(grade, "A") == 0) { return 1u; }
    if (grade != NULL && _stricmp(grade, "B") == 0) { return 2u; }
    if (grade != NULL && _stricmp(grade, "C") == 0) { return 3u; }
    return 4u;
}

static uint32_t kbo_fa_market_display_team_id(const KboFaMarketClassification* row)
{
    if (row == NULL) {
        return 0u;
    }
    if (row->fa_grade_snapshot_team_id != 0u) {
        return row->fa_grade_snapshot_team_id;
    }
    if (row->original_team_id != 0u) {
        return row->original_team_id;
    }
    if (row->active_team_id != 0u) {
        return row->active_team_id;
    }
    return row->current_team_id;
}

static void kbo_fa_market_format_salary(int32_t salary, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (salary <= 0) {
        snprintf(out, out_size, "-");
        return;
    }

    char raw[32] = {0};
    char formatted[48] = {0};
    snprintf(raw, sizeof(raw), "%d", salary);
    size_t raw_len = strlen(raw);
    size_t pos = 0;
    for (size_t i = 0; i < raw_len && pos + 1 < sizeof(formatted); i++) {
        if (i > 0 && ((raw_len - i) % 3u) == 0u && pos + 1 < sizeof(formatted)) {
            formatted[pos++] = ',';
        }
        formatted[pos++] = raw[i];
    }
    formatted[pos] = '\0';
    snprintf(out, out_size, "%s", formatted);
}

static const char* kbo_fa_market_display_case_label(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return "-";
    }
    if (strcmp(case_label, "KBO_FA_APPROVED") == 0) { return "Official FA"; }
    if (strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) { return "Eligible FA"; }
    if (strcmp(case_label, "KBO_FA_DEFERRED") == 0) { return "Deferred FA"; }
    if (strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0) { return "FA"; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_LOCKED") == 0) { return "Requal Lock"; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0) { return "Requal FA"; }
    if (strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0) { return "Released"; }
    if (strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0) { return "Undrafted"; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0) { return "Indy FA"; }
    if (strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0) { return "Teamless"; }
    if (strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) { return "Teamless"; }
    if (strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0) { return "Foreign Rights"; }
    if (strcmp(case_label, "FOREIGN_FREE") == 0) { return "Foreign FA"; }
    return case_label;
}

static int kbo_fa_market_apply_age_grade_override(KboFaMarketClassification* row, const KboFaRules* rules)
{
    if (row == NULL
            || rules == NULL
            || !rules->age_grade_override_enabled
            || rules->age_grade_min_age == 0u
            || rules->age_grade[0] == '\0'
            || (rules->exclude_foreign_players && row->foreign_player)
            || row->age < rules->age_grade_min_age
            || !kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return 0;
    }

    if (_stricmp(row->grade, rules->age_grade) != 0) {
        snprintf(row->grade, sizeof(row->grade), "%s", rules->age_grade);
        row->fa_grade_auto = 1u;
        char previous_reason[224] = {0};
        snprintf(previous_reason, sizeof(previous_reason), "%s", row->reason);
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s; age >= %u FA grade override=%s",
            previous_reason,
            rules->age_grade_min_age,
            rules->age_grade);
    }
    snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AGE_%u_%s", rules->age_grade_min_age, rules->age_grade);
    return 1;
}

static void kbo_fa_market_apply_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRules* rules)
{
    KboFaRules local_rules;
    if (rules == NULL) {
        kbo_fa_rules_load(&local_rules);
        rules = &local_rules;
    }

    if (row == NULL || !kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return;
    }
    if ((rules->exclude_foreign_players && row->foreign_player)
            || strcmp(row->case_label, "FOREIGN_FREE") == 0
            || strcmp(row->case_label, "FOREIGN_RESERVED_RIGHT") == 0) {
        return;
    }

    int age_grade_override = kbo_fa_market_apply_age_grade_override(row, rules);

    const KboFaSalarySnapshotGrade* grade =
        kbo_find_fa_salary_snapshot_grade(salary_grades, salary_grade_count, row->player_id);
    if (grade == NULL) {
        if (!age_grade_override) {
            snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "SNAPSHOT_MISSING");
        }
        return;
    }

    row->fa_grade_salary = grade->salary;
    row->fa_grade_overall_rank = grade->overall_rank;
    row->fa_grade_team_rank = grade->team_rank;
    row->fa_grade_snapshot_team_id = grade->ranking_team_id;
    row->fa_grade_snapshot_date = grade->snapshot_date;
    row->fa_grade_opening_day = grade->opening_day;
    if (!age_grade_override
            && kbo_fa_market_grade_is_unknown(row->grade)
            && !kbo_fa_market_grade_is_unknown(grade->grade)) {
        snprintf(row->grade, sizeof(row->grade), "%s", grade->grade);
        row->fa_grade_auto = 1u;
        char previous_reason[224] = {0};
        snprintf(previous_reason, sizeof(previous_reason), "%s", row->reason);
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s; opening-day salary grade=%s salary=%d overall_rank=%u team_rank=%u",
            previous_reason,
            row->grade,
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank);
    }

    if (row->original_team_id != 0u
            && grade->ranking_team_id != 0u
            && row->original_team_id != grade->ranking_team_id) {
        row->fa_grade_team_changed_review = 1u;
        snprintf(
            row->fa_grade_flag,
            sizeof(row->fa_grade_flag),
            "TEAM_CHANGED_REVIEW");
    } else if (age_grade_override) {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AGE_%u_%s", rules->age_grade_min_age, rules->age_grade);
    } else if (row->fa_grade_auto) {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AUTO");
    } else {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "SNAPSHOT");
    }
}
