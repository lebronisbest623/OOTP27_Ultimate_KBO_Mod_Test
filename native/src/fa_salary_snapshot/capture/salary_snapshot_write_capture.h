#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_WRITE_CAPTURE_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_WRITE_CAPTURE_H_

#include <stddef.h>
#include <stdint.h>

#include "../state/salary_snapshot_state.h"

int kbo_fa_salary_snapshot_write_csv(
    const KboFaSalarySnapshotRow* rows,
    int row_count,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    const char* source,
    char* out_path,
    size_t out_path_size);

int kbo_capture_fa_salary_opening_day_snapshot(
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id);

#endif
