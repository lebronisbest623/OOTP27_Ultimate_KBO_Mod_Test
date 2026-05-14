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
#include "../../core/logging/rule_audit.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/league_news/core_sql_league_news.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../fa_salary_snapshot/state/salary_snapshot_state.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"
#include "../api/competitive_balance_tax.h"
#include "../draft/cbt_draft_probe.h"
#include "../exceptions/cbt_exceptions.h"
#include "../records/cbt_records.h"
#include "../rules/cbt_rules.h"

#define KBO_CBT_TEAM_MAX 32
#define KBO_CBT_SALARY_SCRATCH_MAX 200

typedef struct KboCbtTeamPayroll {
    uint32_t team_id;
    int32_t  domestic_salaries[KBO_CBT_SALARY_SCRATCH_MAX];
    int      domestic_count;
    int32_t  top_n_sum;
    int32_t  exception_credit;
} KboCbtTeamPayroll;

int kbo_cbt_news_date(
    uint32_t season,
    uint32_t news_yyyymmdd,
    uint32_t* out_year,
    uint32_t* out_month,
    uint32_t* out_day);
void kbo_cbt_copy_team_name(uint32_t team_id, char* out, size_t out_size);
void kbo_cbt_insert_violation_news_v2(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const KboCbtRecord* rec,
    const KboCbtRules* rules);
int kbo_cbt_insert_opening_day_summary_news(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t season,
    const KboCbtRecord* records,
    int record_count,
    int team_count,
    const KboCbtRules* rules);
int kbo_cbt_news_marker_exists(const char* key);
void kbo_cbt_news_persist_marker(const char* key, const char* source);

#endif
