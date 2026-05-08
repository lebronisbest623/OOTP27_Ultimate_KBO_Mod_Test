#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_ROW_RANKING_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_ROW_RANKING_H_

#include <stdint.h>

#include "salary_snapshot_state.h"

int32_t kbo_fa_salary_snapshot_player_salary_for_season(uint8_t* player, uint32_t season);
void kbo_fa_salary_snapshot_copy_contract_years(uint8_t* player, KboFaSalarySnapshotRow* row);
int __cdecl kbo_fa_salary_snapshot_compare_overall(const void* a, const void* b);
void kbo_fa_salary_snapshot_assign_overall_ranks(KboFaSalarySnapshotRow* rows, int count);
void kbo_fa_salary_snapshot_assign_team_ranks(KboFaSalarySnapshotRow* rows, int count);

#endif
