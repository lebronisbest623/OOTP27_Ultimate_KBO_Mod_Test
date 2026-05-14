#include "cbt_rule_audit.h"

#include "../../core/logging/rule_audit.h"

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
    int record_count)
{
    kbo_rule_audit_emitf(
        "competitive_balance_tax.process",
        decision,
        reason,
        source,
        "\"season\":%u,\"news_date\":%u,\"threshold\":%d,\"grade_count\":%d,"
        "\"team_count\":%d,\"exception_count\":%d,\"league_id\":%u,"
        "\"violations\":%d,\"record_count\":%d",
        season,
        news_yyyymmdd,
        threshold,
        grade_count,
        team_count,
        exception_count,
        league_id,
        violations,
        record_count);
}

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
    int32_t exception_credit)
{
    kbo_rule_audit_emitf(
        "competitive_balance_tax.process.team",
        decision,
        reason,
        source,
        "\"season\":%u,\"team_id\":%u,\"payroll\":%d,\"threshold\":%d,"
        "\"overage\":%d,\"tax_rate\":%u,\"tax_amount\":%d,\"consecutive\":%u,"
        "\"draft_penalty\":%d,\"exception_credit\":%d",
        season,
        team_id,
        payroll,
        threshold,
        overage,
        tax_rate,
        tax_amount,
        consecutive_count,
        draft_penalty,
        exception_credit);
}

void kbo_cbt_audit_summary_news(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t season,
    uint32_t league_id,
    int team_count,
    int violations)
{
    kbo_rule_audit_emitf(
        "competitive_balance_tax.process.summary_news",
        decision,
        reason,
        source,
        "\"season\":%u,\"league_id\":%u,\"team_count\":%d,\"violations\":%d",
        season,
        league_id,
        team_count,
        violations);
}

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
    int ready)
{
    kbo_rule_audit_emitf(
        "competitive_balance_tax.event_schedule",
        decision,
        reason,
        source,
        "\"season\":%u,\"date\":%u,\"league_id\":%u,\"opening_day\":%u,"
        "\"deadline\":%u,\"announcement\":%u,\"created_deadline\":%d,"
        "\"created_announcement\":%d,\"pruned_deadline\":%d,"
        "\"pruned_announcement\":%d,\"ready\":%d",
        season,
        today_yyyymmdd,
        league_id,
        opening_day,
        deadline,
        announcement,
        created_deadline,
        created_announcement,
        pruned_deadline,
        pruned_announcement,
        ready);
}

void kbo_cbt_audit_event_handler(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t event_yyyymmdd,
    uint32_t season)
{
    kbo_rule_audit_emitf(
        "competitive_balance_tax.event_handler",
        decision,
        reason,
        source,
        "\"event_date\":%u,\"season\":%u",
        event_yyyymmdd,
        season);
}
