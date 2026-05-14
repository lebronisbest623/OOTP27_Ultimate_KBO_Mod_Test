#include "../internal/fa_market_classification_internal.h"

int kbo_fa_market_row_is_undrafted_domestic(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->generation_context != 0u
            || row->generation_grade == 0u) {
        return 0;
    }

    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (row->draft_league_id == (uint32_t)policy->undrafted_college_league_id
            && row->draft_subtype == (uint8_t)policy->undrafted_college_draft_subtype
            && row->age <= (uint16_t)policy->undrafted_college_age_max) {
        return 1;
    }
    if (row->draft_league_id == (uint32_t)policy->undrafted_high_school_league_id
            && row->age <= (uint16_t)policy->undrafted_high_school_age_max) {
        return 1;
    }
    return 0;
}

int kbo_fa_market_row_is_independent_league_fa(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->original_team_id == 0u) {
        return 0;
    }

    if (kbo_fa_market_row_is_undrafted_domestic(row)) {
        return 0;
    }

    uint32_t original_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    uint32_t independent_league_id = (uint32_t)policy->independent_league_id;
    if (original_league_id == independent_league_id) {
        return 1;
    }
    return original_league_id == 0u && row->draft_league_id == independent_league_id;
}

void kbo_fa_market_set_history_reason(
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

int kbo_fa_market_apply_history_case(
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

int kbo_fa_market_grade_is_unknown(const char* grade)
{
    return grade == NULL
        || grade[0] == '\0'
        || _stricmp(grade, "UNKNOWN") == 0
        || strcmp(grade, "-") == 0;
}

uint32_t kbo_fa_market_display_grade_sort_rank(const char* grade)
{
    if (grade != NULL && _stricmp(grade, "A") == 0) { return 1u; }
    if (grade != NULL && _stricmp(grade, "B") == 0) { return 2u; }
    if (grade != NULL && _stricmp(grade, "C") == 0) { return 3u; }
    return 4u;
}

uint32_t kbo_fa_market_display_team_id(const KboFaMarketClassification* row)
{
    if (row == NULL) {
        return 0u;
    }
    if (row->fa_grade_snapshot_team_id != 0u) {
        return row->fa_grade_snapshot_team_id;
    }
    if (row->rights_team_id != 0u) {
        return row->rights_team_id;
    }
    if (row->original_team_id != 0u
            && (strcmp(row->case_label, "KBO_FA_APPROVED") == 0
                || strcmp(row->case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
                || strcmp(row->case_label, "KBO_FA_DEFERRED") == 0
                || strcmp(row->case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
                || strcmp(row->case_label, "KBO_REQUALIFICATION_LOCKED") == 0
                || strcmp(row->case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0
                || strcmp(row->case_label, "DOMESTIC_RELEASED_NON_FA") == 0
                || strcmp(row->case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0)) {
        return row->original_team_id;
    }
    return row->current_team_id;
}

void kbo_fa_market_format_salary(int32_t salary, char* out, size_t out_size)
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

int kbo_fa_market_apply_age_grade_override(KboFaMarketClassification* row, const KboFaRules* rules)
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

typedef struct KboHistoricalFaSeedRecord {
    uint32_t season;
    uint32_t fa_round;
    char player_key[64];
    char grade[12];
} KboHistoricalFaSeedRecord;

#define KBO_HISTORICAL_FA_SEED_MAX 1024

static KboHistoricalFaSeedRecord g_kbo_historical_fa_seed[KBO_HISTORICAL_FA_SEED_MAX];
static int g_kbo_historical_fa_seed_count = 0;
static volatile LONG g_kbo_historical_fa_seed_loaded = 0;
static volatile LONG g_kbo_historical_fa_seed_lock = 0;

static int kbo_historical_fa_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("historical_fa_seed.csv", out, out_size);
}

static int kbo_fa_grade_is_abc(const char* grade)
{
    return grade != NULL
        && (strcmp(grade, "A") == 0 || strcmp(grade, "B") == 0 || strcmp(grade, "C") == 0);
}

static int kbo_load_historical_fa_seed_once(void)
{
    if (InterlockedCompareExchange(&g_kbo_historical_fa_seed_loaded, 0, 0) != 0) {
        return g_kbo_historical_fa_seed_count;
    }
    while (InterlockedCompareExchange(&g_kbo_historical_fa_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
    if (g_kbo_historical_fa_seed_loaded != 0) {
        InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
        return g_kbo_historical_fa_seed_count;
    }

    memset(g_kbo_historical_fa_seed, 0, sizeof(g_kbo_historical_fa_seed));
    char path[MAX_PATH] = {0};
    if (!kbo_historical_fa_seed_path(path, sizeof(path))) {
        InterlockedExchange(&g_kbo_historical_fa_seed_loaded, 1);
        InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        InterlockedExchange(&g_kbo_historical_fa_seed_loaded, 1);
        InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
        return 0;
    }

    int count = 0;
    while (count < KBO_HISTORICAL_FA_SEED_MAX && kbo_csv_reader_next_row(reader)) {
        char fields[18][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 18);
        if (field_count <= 8 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }
        uint32_t season = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t fa_round = kbo_csv_parse_u32_text(fields[1], 10);
        if (season < 1982u || season > 2200u || fa_round == 0u || fields[2][0] == '\0') {
            continue;
        }
        g_kbo_historical_fa_seed[count].season = season;
        g_kbo_historical_fa_seed[count].fa_round = fa_round;
        snprintf(g_kbo_historical_fa_seed[count].player_key, sizeof(g_kbo_historical_fa_seed[count].player_key), "%s", fields[2]);
        snprintf(g_kbo_historical_fa_seed[count].grade, sizeof(g_kbo_historical_fa_seed[count].grade), "%s", fields[8]);
        count++;
    }
    kbo_csv_reader_close(reader);
    g_kbo_historical_fa_seed_count = count;
    InterlockedExchange(&g_kbo_historical_fa_seed_loaded, 1);
    InterlockedExchange(&g_kbo_historical_fa_seed_lock, 0);
    return count;
}

static const KboHistoricalFaSeedRecord* kbo_find_latest_historical_fa_seed_before(
    const char* player_key,
    uint32_t current_year)
{
    if (player_key == NULL || player_key[0] == '\0' || current_year == 0u) {
        return NULL;
    }
    int count = kbo_load_historical_fa_seed_once();
    const KboHistoricalFaSeedRecord* best = NULL;
    for (int i = 0; i < count; i++) {
        const KboHistoricalFaSeedRecord* rec = &g_kbo_historical_fa_seed[i];
        if (rec->season < current_year
                && strcmp(rec->player_key, player_key) == 0
                && (best == NULL || rec->season > best->season)) {
            best = rec;
        }
    }
    return best;
}

static int kbo_fa_market_apply_requalification_grade_override(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grade,
    const KboFaRequalificationRecord* requalification_records,
    int requalification_count,
    uint32_t current_year,
    const KboFaRules* rules)
{
    if (row == NULL
            || rules == NULL
            || !kbo_fa_rules_case_is_compensable(rules, row->case_label)
            || (rules->exclude_foreign_players && row->foreign_player)) {
        return 0;
    }

    uint32_t prior_fa_count = 0u;
    const char* previous_grade = NULL;
    const char* source = NULL;
    uint32_t previous_year = 0u;
    const KboFaRequalificationRecord* rec =
        kbo_find_fa_market_requalification_record(requalification_records, requalification_count, row->player_id);
    if (rec != NULL) {
        prior_fa_count = rec->fa_count;
        previous_grade = rec->last_fa_grade;
        previous_year = rec->last_fa_year;
        source = "runtime";
    }

    if ((previous_grade == NULL || !kbo_fa_grade_is_abc(previous_grade)) && salary_grade != NULL) {
        const KboHistoricalFaSeedRecord* history =
            kbo_find_latest_historical_fa_seed_before(salary_grade->player_key, current_year);
        if (history != NULL) {
            prior_fa_count = history->fa_round;
            previous_grade = history->grade;
            previous_year = history->season;
            source = "historical_fa_seed";
        }
    }

    const char* override_grade = NULL;
    if (prior_fa_count >= 2u) {
        override_grade = "C";
    } else if (previous_grade != NULL && strcmp(previous_grade, "C") == 0) {
        override_grade = "C";
    } else if (previous_grade != NULL && (strcmp(previous_grade, "A") == 0 || strcmp(previous_grade, "B") == 0)) {
        override_grade = "B";
    }

    if (override_grade == NULL) {
        return 0;
    }

    if (_stricmp(row->grade, override_grade) != 0) {
        char previous_reason[224] = {0};
        snprintf(previous_reason, sizeof(previous_reason), "%s", row->reason);
        snprintf(row->grade, sizeof(row->grade), "%s", override_grade);
        row->fa_grade_auto = 1u;
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s; FA requalification grade=%s previous_grade=%s previous_count=%u previous_year=%u source=%s",
            previous_reason,
            override_grade,
            previous_grade != NULL && previous_grade[0] != '\0' ? previous_grade : "UNKNOWN",
            prior_fa_count,
            previous_year,
            source != NULL ? source : "unknown");
    }
    snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "REQUAL_%s", override_grade);
    return 1;
}

void kbo_fa_market_apply_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRequalificationRecord* requalification_records,
    int requalification_count,
    uint32_t current_year,
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
        int requalification_grade_override = kbo_fa_market_apply_requalification_grade_override(
            row,
            NULL,
            requalification_records,
            requalification_count,
            current_year,
            rules);
        if (!age_grade_override && !requalification_grade_override) {
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

    kbo_fa_market_apply_requalification_grade_override(
        row,
        grade,
        requalification_records,
        requalification_count,
        current_year,
        rules);
}

void kbo_fa_market_mark_history_case(KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_text[0] == '\0') {
        return;
    }
    history->became_free_agent = strcmp(history->history_text, "[G]Became a free agent.") == 0;
    history->undrafted_free_agent =
        strstr(history->history_text, "Was not drafted and became a free agent") != NULL;
    history->released = strncmp(history->history_text, "[G]Released by ", 15) == 0;
}

