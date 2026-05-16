#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_PROCESS_PAYROLL_COMPUTE_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_PROCESS_PAYROLL_COMPUTE_H_

#include "../../internal/cbt_internal.h"

void kbo_cbt_compute_team_payrolls(
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    uint32_t season,
    const KboCbtExceptionDesignation* exceptions,
    int exception_count,
    uint32_t top_n,
    KboCbtTeamPayroll* teams,
    int* team_count_out);

#endif