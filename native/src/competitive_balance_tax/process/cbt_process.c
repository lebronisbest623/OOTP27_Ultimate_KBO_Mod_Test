#include "../internal/cbt_internal.h"

static int __cdecl kbo_cbt_salary_compare_desc(const void* a, const void* b)
{
    int32_t la = *(const int32_t*)a;
    int32_t lb = *(const int32_t*)b;
    return lb > la ? 1 : (lb < la ? -1 : 0);
}

static int kbo_cbt_find_team(const KboCbtTeamPayroll* teams, int count, uint32_t team_id)
{
    for (int i = 0; i < count; i++) {
        if (teams[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static void kbo_cbt_compute_team_payrolls(
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    uint32_t top_n,
    KboCbtTeamPayroll* teams,
    int* team_count_out)
{
    *team_count_out = 0;

    KboCbtTeamPayroll scratch[KBO_CBT_TEAM_MAX];
    memset(scratch, 0, sizeof(scratch));
    int team_count = 0;

    for (int i = 0; i < grade_count; i++) {
        const KboFaSalarySnapshotGrade* g = &grades[i];
        if (g->ranking_team_id == 0u || g->foreign_flag || g->salary <= 0) {
            continue;
        }

        int ti = kbo_cbt_find_team(scratch, team_count, g->ranking_team_id);
        if (ti < 0) {
            if (team_count >= KBO_CBT_TEAM_MAX) {
                continue;
            }
            ti = team_count++;
            scratch[ti].team_id = g->ranking_team_id;
        }

        KboCbtTeamPayroll* tp = &scratch[ti];
        if (tp->domestic_count < KBO_CBT_SALARY_SCRATCH_MAX) {
            tp->domestic_salaries[tp->domestic_count++] = g->salary;
        }
    }

    for (int i = 0; i < team_count; i++) {
        KboCbtTeamPayroll* tp = &scratch[i];
        qsort(tp->domestic_salaries, (size_t)tp->domestic_count,
            sizeof(tp->domestic_salaries[0]), kbo_cbt_salary_compare_desc);

        int take = tp->domestic_count < (int)top_n ? tp->domestic_count : (int)top_n;
        int32_t sum = 0;
        for (int j = 0; j < take; j++) {
            sum += tp->domestic_salaries[j];
        }
        tp->top_n_sum = sum;
        teams[i] = *tp;
    }
    *team_count_out = team_count;
}

static void __attribute__((unused)) kbo_cbt_insert_violation_news(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const KboCbtRecord* rec,
    const KboCbtRules* rules)
{
    if (league_id == 0u || rec == NULL || rules == NULL) {
        return;
    }

    char title[256] = {0};
    snprintf(title, sizeof(title),
        "[경쟁균형세] %s 초과 — %u시즌",
        rec->team_name[0] != '\0' ? rec->team_name : "팀",
        rec->season);

    int draft_penalty = (int)rec->consecutive_count >= (int)rules->draft_penalty_min_consecutive;

    char body[1024] = {0};
    snprintf(body, sizeof(body),
        "%s 구단이 %u 시즌 경쟁균형세 상한(%d)을 초과하였습니다.\n\n"
        "페이롤: %d\n"
        "초과액: %d\n"
        "세율: %u%%\n"
        "납부액: %d\n"
        "연속 초과: %u회%s",
        rec->team_name[0] != '\0' ? rec->team_name : "해당 팀",
        rec->season,
        rec->threshold,
        rec->payroll,
        rec->overage,
        rec->tax_rate_pct,
        rec->tax_amount,
        rec->consecutive_count,
        draft_penalty
            ? "\n\n드래프트 1라운드 지명권 9단계 하락 페널티 적용 (수동 조정 필요)"
            : "");

    insert_kbo_league_news_sql(year, month, day, league_id, 2u, title, body, "cbt_violation");
    insert_kbo_league_news_table_sql(year, month, day, league_id, title, body, "cbt_violation");
}

static void kbo_cbt_insert_violation_news_v2(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const KboCbtRecord* rec,
    const KboCbtRules* rules)
{
    if (league_id == 0u || rec == NULL || rules == NULL) {
        return;
    }

    const char* team_name = rec->team_name[0] != '\0' ? rec->team_name : "Team";

    char title[256] = {0};
    snprintf(title, sizeof(title),
        "[KBO CBT] %s exceeds %u cap",
        team_name,
        rec->season);

    int draft_penalty = (int)rec->consecutive_count >= (int)rules->draft_penalty_min_consecutive;

    char penalty_text[192] = {0};
    if (draft_penalty) {
        snprintf(penalty_text, sizeof(penalty_text),
            "\n\nDraft penalty: amateur assignment priority is lowered by %u stages.",
            rules->draft_penalty_stages);
    }

    char body[1024] = {0};
    snprintf(body, sizeof(body),
        "%s exceeded the KBO competitive balance tax threshold for the %u season.\n\n"
        "Payroll: %d\n"
        "Threshold: %d\n"
        "Overage: %d\n"
        "Tax rate: %u%%\n"
        "Tax due: %d\n"
        "Consecutive overage seasons: %u%s",
        team_name,
        rec->season,
        rec->payroll,
        rec->threshold,
        rec->overage,
        rec->tax_rate_pct,
        rec->tax_amount,
        rec->consecutive_count,
        penalty_text);

    insert_kbo_league_news_sql(year, month, day, league_id, 2u, title, body, "cbt_violation");
    insert_kbo_league_news_table_sql(year, month, day, league_id, title, body, "cbt_violation");
}

void kbo_process_competitive_balance_tax(uint32_t season, const char* source)
{
    if (season < 1982u || season > 2200u) {
        return;
    }

    if (read_kbo_localappdata_flag_file("disable_kbo_competitive_balance_tax.txt")) {
        append_logf("KBO CBT skipped season=%u source=%s reason=flag_disabled", season, source != NULL ? source : "");
        return;
    }

    /* One-shot schema probe so we can discover the draft pick table structure */
    static volatile LONG probe_done = 0;
    if (InterlockedCompareExchange(&probe_done, 1, 0) == 0) {
        uintptr_t global = get_ootp_global_database();
        if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
            uintptr_t db = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
            if (db != 0) {
                kbo_cbt_draft_probe_schema((void*)db);
            }
        }
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    if (!rules.enabled) {
        append_logf("KBO CBT skipped season=%u source=%s reason=disabled", season, source != NULL ? source : "");
        return;
    }

    int32_t threshold = kbo_cbt_get_threshold(&rules, season);
    if (threshold <= 0) {
        append_logf("KBO CBT skipped season=%u source=%s reason=no_threshold", season, source != NULL ? source : "");
        return;
    }

    /* Load salary snapshot for this season */
    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (grades == NULL) {
        return;
    }

    int grade_count = kbo_fa_salary_snapshot_load_grade_rows(season, grades, KBO_FA_SALARY_SNAPSHOT_GRADE_MAX, NULL, 0);
    if (grade_count <= 0) {
        append_logf("KBO CBT skipped season=%u source=%s reason=no_salary_data", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, grades);
        return;
    }

    /* Compute per-team domestic payroll (top N) */
    KboCbtTeamPayroll teams[KBO_CBT_TEAM_MAX];
    memset(teams, 0, sizeof(teams));
    int team_count = 0;
    kbo_cbt_compute_team_payrolls(grades, grade_count, rules.top_player_count, teams, &team_count);
    HeapFree(GetProcessHeap(), 0, grades);

    if (team_count == 0) {
        append_logf("KBO CBT skipped season=%u source=%s reason=no_teams", season, source != NULL ? source : "");
        return;
    }

    /* Load existing records to check consecutive history */
    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) {
        return;
    }
    int record_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);

    uint32_t year = season;
    uint32_t month = 4u;
    uint32_t day = 1u;
    uint32_t league_id = kbo_resolve_kbo_league_id();

    int new_violations = 0;
    for (int i = 0; i < team_count; i++) {
        uint32_t team_id = teams[i].team_id;
        int32_t payroll = teams[i].top_n_sum;

        /* Skip if already processed this season for this team */
        if (kbo_cbt_find_record(records, record_count, season, team_id) >= 0) {
            continue;
        }

        int32_t overage = payroll - threshold;
        uint32_t prior_consecutive = kbo_cbt_get_consecutive_count(records, record_count, team_id, season);
        uint32_t consecutive_count = overage > 0 ? prior_consecutive + 1u : 0u;
        uint32_t tax_rate = kbo_cbt_get_tax_rate(&rules, consecutive_count);
        int32_t tax_amount = overage > 0
            ? (int32_t)((int64_t)overage * (int32_t)tax_rate / 100LL)
            : 0;

        KboCbtRecord rec = {0};
        rec.season           = season;
        rec.team_id          = team_id;
        rec.payroll          = payroll;
        rec.threshold        = threshold;
        rec.overage          = overage > 0 ? overage : 0;
        rec.tax_rate_pct     = tax_rate;
        rec.tax_amount       = tax_amount;
        rec.consecutive_count = consecutive_count;
        rec.processed_date   = year * 10000u + month * 100u + day;

        if (record_count < KBO_CBT_RECORDS_MAX) {
            records[record_count++] = rec;
        }

        if (overage > 0) {
            new_violations++;
            int draft_penalty = (int)consecutive_count >= (int)rules.draft_penalty_min_consecutive;
            append_logf(
                "KBO CBT violation season=%u team=%u payroll=%d threshold=%d overage=%d rate=%u%% tax=%d consecutive=%u draft_penalty=%d source=%s",
                season, team_id, payroll, threshold, overage,
                tax_rate, tax_amount, consecutive_count, draft_penalty,
                source != NULL ? source : "");

            if (league_id != 0u) {
                kbo_cbt_insert_violation_news_v2(league_id, year, month, day, &rec, &rules);
            }
        } else {
            append_logf(
                "KBO CBT clean season=%u team=%u payroll=%d threshold=%d source=%s",
                season, team_id, payroll, threshold,
                source != NULL ? source : "");
        }
    }

    kbo_cbt_save_records(records, record_count);
    HeapFree(GetProcessHeap(), 0, records);

    append_logf(
        "KBO CBT processed season=%u teams=%d violations=%d threshold=%d source=%s",
        season, team_count, new_violations, threshold,
        source != NULL ? source : "");
}
