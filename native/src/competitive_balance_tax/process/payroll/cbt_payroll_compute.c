#include "cbt_payroll_compute.h"

static int kbo_cbt_find_team(const KboCbtTeamPayroll* teams, int count, uint32_t team_id)
{
    for (int i = 0; i < count; i++) {
        if (teams[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static uint32_t kbo_cbt_effective_top_n(uint32_t top_n)
{
    if (top_n == 0u) {
        return 1u;
    }
    if (top_n > KBO_CBT_SALARY_SCRATCH_MAX) {
        return KBO_CBT_SALARY_SCRATCH_MAX;
    }
    return top_n;
}

static void kbo_cbt_track_top_salary(KboCbtTeamPayroll* team, int32_t salary, uint32_t top_n)
{
    if (team == NULL || salary <= 0) {
        return;
    }

    if (team->domestic_count < (int)top_n) {
        team->domestic_salaries[team->domestic_count++] = salary;
        return;
    }

    int min_index = 0;
    int32_t min_salary = team->domestic_salaries[0];
    for (int i = 1; i < team->domestic_count; i++) {
        if (team->domestic_salaries[i] < min_salary) {
            min_salary = team->domestic_salaries[i];
            min_index = i;
        }
    }

    if (salary > min_salary) {
        team->domestic_salaries[min_index] = salary;
    }
}

static int32_t kbo_cbt_adjust_salary_for_exception(
    const KboFaSalarySnapshotGrade* grade,
    const KboCbtExceptionDesignation* exceptions,
    int exception_count,
    uint32_t season,
    int32_t* out_credit)
{
    if (out_credit != NULL) {
        *out_credit = 0;
    }
    if (grade == NULL || grade->salary <= 0 || grade->player_key[0] == '\0') {
        return grade != NULL ? grade->salary : 0;
    }
    if (kbo_cbt_exception_find_designation(
            exceptions,
            exception_count,
            season,
            grade->ranking_team_id,
            grade->player_key) < 0) {
        return grade->salary;
    }
    int32_t credit = grade->salary / 2;
    if (out_credit != NULL) {
        *out_credit = credit;
    }
    return grade->salary - credit;
}

void kbo_cbt_compute_team_payrolls(
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    uint32_t season,
    const KboCbtExceptionDesignation* exceptions,
    int exception_count,
    uint32_t top_n,
    KboCbtTeamPayroll* teams,
    int* team_count_out)
{
    *team_count_out = 0;

    uint32_t effective_top_n = kbo_cbt_effective_top_n(top_n);
    KboCbtTeamPayroll* scratch = (KboCbtTeamPayroll*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_TEAM_MAX * sizeof(KboCbtTeamPayroll));
    if (scratch == NULL) {
        kbo_log_runtime_line("KBO CBT skipped reason=team_scratch_alloc_failed");
        return;
    }

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
        int32_t exception_credit = 0;
        int32_t adjusted_salary = kbo_cbt_adjust_salary_for_exception(
            g,
            exceptions,
            exception_count,
            season,
            &exception_credit);
        tp->exception_credit += exception_credit;
        kbo_cbt_track_top_salary(tp, adjusted_salary, effective_top_n);
    }

    for (int i = 0; i < team_count; i++) {
        KboCbtTeamPayroll* tp = &scratch[i];
        int32_t sum = 0;
        for (int j = 0; j < tp->domestic_count; j++) {
            sum += tp->domestic_salaries[j];
        }
        tp->top_n_sum = sum;
        teams[i] = *tp;
    }
    *team_count_out = team_count;
    HeapFree(GetProcessHeap(), 0, scratch);
}

