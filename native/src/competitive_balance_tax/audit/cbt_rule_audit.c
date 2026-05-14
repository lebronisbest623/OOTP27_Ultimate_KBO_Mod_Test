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
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "news_date", news_yyyymmdd);
        kbo_log_field_i32(&audit_fields, "threshold", threshold);
        kbo_log_field_i32(&audit_fields, "grade_count", grade_count);
        kbo_log_field_i32(&audit_fields, "team_count", team_count);
        kbo_log_field_i32(&audit_fields, "exception_count", exception_count);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "violations", violations);
        kbo_log_field_i32(&audit_fields, "record_count", record_count);
        kbo_rule_audit_emit_fields(
            "competitive_balance_tax.process",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
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
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "team_id", team_id);
        kbo_log_field_i32(&audit_fields, "payroll", payroll);
        kbo_log_field_i32(&audit_fields, "threshold", threshold);
        kbo_log_field_i32(&audit_fields, "overage", overage);
        kbo_log_field_u32(&audit_fields, "tax_rate", tax_rate);
        kbo_log_field_i32(&audit_fields, "tax_amount", tax_amount);
        kbo_log_field_u32(&audit_fields, "consecutive", consecutive_count);
        kbo_log_field_i32(&audit_fields, "draft_penalty", draft_penalty);
        kbo_log_field_i32(&audit_fields, "exception_credit", exception_credit);
        kbo_rule_audit_emit_fields(
            "competitive_balance_tax.process.team",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
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
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "team_count", team_count);
        kbo_log_field_i32(&audit_fields, "violations", violations);
        kbo_rule_audit_emit_fields(
            "competitive_balance_tax.process.summary_news",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
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
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_u32(&audit_fields, "opening_day", opening_day);
        kbo_log_field_u32(&audit_fields, "deadline", deadline);
        kbo_log_field_u32(&audit_fields, "announcement", announcement);
        kbo_log_field_i32(&audit_fields, "created_deadline", created_deadline);
        kbo_log_field_i32(&audit_fields, "created_announcement", created_announcement);
        kbo_log_field_i32(&audit_fields, "pruned_deadline", pruned_deadline);
        kbo_log_field_i32(&audit_fields, "pruned_announcement", pruned_announcement);
        kbo_log_field_i32(&audit_fields, "ready", ready);
        kbo_rule_audit_emit_fields(
            "competitive_balance_tax.event_schedule",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}

void kbo_cbt_audit_event_handler(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t event_yyyymmdd,
    uint32_t season)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "event_date", event_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_rule_audit_emit_fields(
            "competitive_balance_tax.event_handler",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}
