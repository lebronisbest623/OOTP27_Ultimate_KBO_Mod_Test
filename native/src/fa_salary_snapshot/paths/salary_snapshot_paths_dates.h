#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_PATHS_DATES_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_PATHS_DATES_H_

#include <stddef.h>
#include <stdint.h>

int kbo_fa_salary_snapshot_path(uint32_t season, char* out, size_t out_size);
int kbo_fa_salary_snapshot_file_exists(uint32_t season);
uint32_t kbo_fa_salary_snapshot_resolve_ranking_team(uint32_t league_id, uint32_t current_team_id, uint32_t active_team_id);
int kbo_fa_salary_snapshot_read_opening_day(uintptr_t league_ptr, uint32_t* out_date);
int kbo_fa_salary_snapshot_current_date_in_opening_window(uint32_t date, uint32_t opening_day);
int kbo_fa_salary_snapshot_load_schedule_opening_day(uint32_t season, uint32_t* out_opening_day);
int kbo_fa_salary_snapshot_today_has_opening_day_message(uint32_t date);

#endif
