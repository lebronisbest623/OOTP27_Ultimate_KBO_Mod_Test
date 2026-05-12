#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_INTERNAL_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/league_news/core_sql_league_news.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../fa_salary_snapshot/state/salary_snapshot_state.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../api/competitive_balance_tax.h"
#include "../draft/cbt_draft_probe.h"
#include "../records/cbt_records.h"
#include "../rules/cbt_rules.h"

#define KBO_CBT_TEAM_MAX 32
#define KBO_CBT_SALARY_SCRATCH_MAX KBO_FA_SALARY_SNAPSHOT_GRADE_MAX

typedef struct KboCbtTeamPayroll {
    uint32_t team_id;
    int32_t  domestic_salaries[KBO_CBT_SALARY_SCRATCH_MAX];
    int      domestic_count;
    int32_t  top_n_sum;
} KboCbtTeamPayroll;

#endif
