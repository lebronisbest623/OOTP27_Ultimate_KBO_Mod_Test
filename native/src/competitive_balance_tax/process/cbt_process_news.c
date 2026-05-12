#include "../internal/cbt_internal.h"

#include <stdarg.h>

static void kbo_cbt_format_usd(char* out, size_t out_size, int32_t value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    if (value <= 0) {
        snprintf(out, out_size, "$0");
        return;
    }
    if (value >= 1000000) {
        int m = value / 1000000;
        int tenth = (value % 1000000) / 100000;
        if (tenth == 0) {
            snprintf(out, out_size, "$%dM", m);
        } else {
            snprintf(out, out_size, "$%d.%dM", m, tenth);
        }
    } else if (value >= 1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
    } else {
        snprintf(out, out_size, "$%d", value);
    }
}

int kbo_cbt_news_date(
    uint32_t season,
    uint32_t news_yyyymmdd,
    uint32_t* out_year,
    uint32_t* out_month,
    uint32_t* out_day)
{
    uint32_t year = news_yyyymmdd / 10000u;
    uint32_t month = (news_yyyymmdd / 100u) % 100u;
    uint32_t day = news_yyyymmdd % 100u;
    if (year < 1982u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        year = season;
        month = 4u;
        day = 1u;
    }
    if (out_year != NULL) {
        *out_year = year;
    }
    if (out_month != NULL) {
        *out_month = month;
    }
    if (out_day != NULL) {
        *out_day = day;
    }
    return 1;
}

void kbo_cbt_copy_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };
        char city[64] = {0};
        char nickname[64] = {0};
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        if (city[0] != '\0' && nickname[0] != '\0' && !ascii_equals_ignore_case(city, nickname)) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
            char text[96] = {0};
            if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text)) && text[0] != '\0') {
                snprintf(out, out_size, "%s", text);
                return;
            }
        }
    }

    snprintf(out, out_size, "Team #%u", team_id);
}

static void kbo_cbt_appendf(char* out, size_t out_size, const char* fmt, ...)
{
    if (out == NULL || out_size == 0 || fmt == NULL) {
        return;
    }
    size_t len = strlen(out);
    if (len + 1u >= out_size) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(out + len, out_size - len, fmt, args);
    va_end(args);
}

void kbo_cbt_insert_violation_news_v2(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const KboCbtRecord* rec,
    const KboCbtRules* rules)
{
    if (league_id == 0u || rec == NULL || rules == NULL) {
        return;
    }

    const char* team_name = rec->team_name[0] != '\0' ? rec->team_name : "Team";

    char title[256] = {0};
    snprintf(title, sizeof(title),
        "[KBO CBT] %s exceeds %u cap",
        team_name,
        rec->season);

    int draft_penalty = (int)rec->consecutive_count >= (int)rules->draft_penalty_min_consecutive;

    char penalty_text[192] = {0};
    if (draft_penalty) {
        snprintf(penalty_text, sizeof(penalty_text),
            "\n\nDraft penalty: amateur assignment priority is lowered by %u stages.",
            rules->draft_penalty_stages);
    }

    char body[1024] = {0};
    snprintf(body, sizeof(body),
        "%s exceeded the KBO competitive balance tax threshold for the %u season.\n\n"
        "Payroll: %d\n"
        "Threshold: %d\n"
        "Overage: %d\n"
        "Tax rate: %u%%\n"
        "Tax due: %d\n"
        "Consecutive overage seasons: %u%s",
        team_name,
        rec->season,
        rec->payroll,
        rec->threshold,
        rec->overage,
        rec->tax_rate_pct,
        rec->tax_amount,
        rec->consecutive_count,
        penalty_text);

    insert_kbo_league_news_sql(year, month, day, league_id, 2u, title, body, "cbt_violation");
    insert_kbo_league_news_table_sql(year, month, day, league_id, title, body, "cbt_violation");
}

void kbo_cbt_insert_opening_day_summary_news(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t season,
    const KboCbtRecord* records,
    int record_count,
    int team_count,
    const KboCbtRules* rules)
{
    if (league_id == 0u || records == NULL || record_count <= 0 || rules == NULL) {
        return;
    }

    int reviewed = 0;
    int violations = 0;
    int32_t total_tax = 0;
    for (int i = 0; i < record_count; i++) {
        if (records[i].season != season) {
            continue;
        }
        reviewed++;
        if (records[i].overage > 0) {
            violations++;
            total_tax += records[i].tax_amount;
        }
    }
    if (reviewed == 0) {
        return;
    }

    char threshold_text[32] = {0};
    char total_tax_text[32] = {0};
    kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), rules->threshold_override > 0 ? rules->threshold_override : kbo_cbt_get_threshold(rules, season));
    kbo_cbt_format_usd(total_tax_text, sizeof(total_tax_text), total_tax);

    char title[256] = {0};
    snprintf(title, sizeof(title), "[KBO CBT] %u Competitive Balance Tax Review", season);

    char body[4096] = {0};
    kbo_cbt_appendf(
        body,
        sizeof(body),
        "On Opening Day, the KBO finalized its %u competitive balance tax review using the opening-day salary snapshot.\n\n"
        "Threshold: %s\n"
        "Payroll basis: top %u domestic salaries per club\n"
        "Clubs reviewed: %d\n"
        "Clubs over threshold: %d\n"
        "Total tax due: %s",
        season,
        threshold_text,
        rules->top_player_count,
        team_count > 0 ? team_count : reviewed,
        violations,
        total_tax_text);

    if (violations <= 0) {
        kbo_cbt_appendf(
            body,
            sizeof(body),
            "\n\nNo clubs exceeded the threshold. No competitive balance tax is due for the %u season.",
            season);
    } else {
        kbo_cbt_appendf(body, sizeof(body), "\n\nOver-threshold clubs:\n");
        for (int i = 0; i < record_count; i++) {
            const KboCbtRecord* rec = &records[i];
            if (rec->season != season || rec->overage <= 0) {
                continue;
            }

            char team_name[96] = {0};
            if (rec->team_name[0] != '\0') {
                snprintf(team_name, sizeof(team_name), "%s", rec->team_name);
            } else {
                kbo_cbt_copy_team_name(rec->team_id, team_name, sizeof(team_name));
            }

            char payroll_text[32] = {0};
            char overage_text[32] = {0};
            char tax_text[32] = {0};
            kbo_cbt_format_usd(payroll_text, sizeof(payroll_text), rec->payroll);
            kbo_cbt_format_usd(overage_text, sizeof(overage_text), rec->overage);
            kbo_cbt_format_usd(tax_text, sizeof(tax_text), rec->tax_amount);
            kbo_cbt_appendf(
                body,
                sizeof(body),
                "- %s: payroll %s, overage %s, tax %s (%u%%, streak %u)\n",
                team_name,
                payroll_text,
                overage_text,
                tax_text,
                rec->tax_rate_pct,
                rec->consecutive_count);
        }

        if (rules->draft_penalty_min_consecutive > 0u) {
            kbo_cbt_appendf(
                body,
                sizeof(body),
                "\nClubs with at least %u consecutive over-threshold seasons also receive a %u-stage amateur assignment priority penalty.",
                rules->draft_penalty_min_consecutive,
                rules->draft_penalty_stages);
        }
    }

    int created = create_kbo_native_live_news_with_body(year, month, day, league_id, 2u, title, body);
    append_logf(
        "KBO CBT opening-day news season=%u date=%04u%02u%02u league_id=%u reviewed=%d violations=%d created=%d",
        season,
        year,
        month,
        day,
        league_id,
        reviewed,
        violations,
        created);
}
