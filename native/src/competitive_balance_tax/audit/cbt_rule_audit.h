#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_AUDIT_CBT_RULE_AUDIT_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_AUDIT_CBT_RULE_AUDIT_H_

#include <stdint.h>

void kbo_cbt_audit_process(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t season,
    uint32_t news_yyyymmdd,
    int32_t threshold,
    int grade_count,
    int team_count,
    int exception_count,
    uint32_t league_id,
    int violations,
    int record_count);

void kbo_cbt_audit_team(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t season,
    uint32_t team_id,
    int32_t payroll,
    int32_t threshold,
    int32_t overage,
    uint32_t tax_rate,
    int32_t tax_amount,
    uint32_t consecutive_count,
    int draft_penalty,
    int32_t exception_credit);

void kbo_cbt_audit_summary_news(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t season,
    uint32_t league_id,
    int team_count,
    int violations);

void kbo_cbt_audit_event_schedule(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t season,
    uint32_t today_yyyymmdd,
    uint32_t league_id,
    uint32_t opening_day,
    uint32_t deadline,
    uint32_t announcement,
    int created_deadline,
    int created_announcement,
    int pruned_deadline,
    int pruned_announcement,
    int ready);

void kbo_cbt_audit_event_handler(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t event_yyyymmdd,
    uint32_t season);

#endif
