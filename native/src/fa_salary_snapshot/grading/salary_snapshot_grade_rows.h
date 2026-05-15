#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_GRADE_ROWS_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_GRADE_ROWS_H_

#include <stddef.h>
#include <stdint.h>

#include "../state/salary_snapshot_state.h"

int kbo_fa_salary_snapshot_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size);
int kbo_fa_salary_snapshot_load_fa_market_grade_rows(
    uint32_t current_year,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size,
    uint32_t* out_grade_season);
const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id);

#endif
