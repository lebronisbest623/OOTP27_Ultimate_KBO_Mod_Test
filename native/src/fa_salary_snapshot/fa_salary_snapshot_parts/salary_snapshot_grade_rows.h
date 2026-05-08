#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_GRADE_ROWS_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_GRADE_ROWS_H_

#include <stddef.h>
#include <stdint.h>

#include "salary_snapshot_state.h"

int kbo_fa_salary_snapshot_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size);
const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id);

#endif
