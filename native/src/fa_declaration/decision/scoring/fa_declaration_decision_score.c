#include "fa_declaration_decision_score.h"

#include <string.h>

static int32_t kbo_fa_declaration_nonnegative_i16(int16_t value)
{
    return value > 0 ? (int32_t)value : 0;
}

int32_t kbo_fa_declaration_current_market_score(const KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return 0;
    }
    int32_t overall = kbo_fa_declaration_nonnegative_i16(candidate->overall);
    int32_t ratings = kbo_fa_declaration_nonnegative_i16(candidate->ratings);
    int32_t career = kbo_fa_declaration_nonnegative_i16(candidate->career);
    return (overall * 45) + (ratings * 35) + (career * 20);
}

int32_t kbo_fa_declaration_upside_score(const KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return 0;
    }
    int32_t talent = kbo_fa_declaration_nonnegative_i16(candidate->talent);
    int32_t career = kbo_fa_declaration_nonnegative_i16(candidate->career);
    int32_t overall = kbo_fa_declaration_nonnegative_i16(candidate->overall);
    int32_t ratings = kbo_fa_declaration_nonnegative_i16(candidate->ratings);
    return (talent * 45) + (career * 25) + (overall * 20) + (ratings * 10);
}

int kbo_fa_declaration_grade_rank(const char* grade)
{
    if (grade == NULL || grade[0] == '\0') {
        return 0;
    }
    if (_stricmp(grade, "A") == 0) { return 3; }
    if (_stricmp(grade, "B") == 0) { return 2; }
    if (_stricmp(grade, "C") == 0) { return 1; }
    return 0;
}

int32_t kbo_fa_declaration_age_threshold_adjustment(uint16_t age)
{
    if (age >= 41u) { return 62000; }
    if (age >= 40u) { return 52000; }
    if (age >= 39u) { return 42000; }
    if (age >= 38u) { return 34000; }
    if (age >= 37u) { return 26000; }
    if (age >= 36u) { return 16000; }
    if (age >= 34u) { return 8000; }
    if (age >= 29u && age <= 31u) { return -4000; }
    return 0;
}

int32_t kbo_fa_declaration_market_score(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score)
{
    if (candidate == NULL) {
        return 0;
    }
    return ((candidate->score * 45) + (current_score * 40) + (upside_score * 15)) / 100;
}

int kbo_fa_declaration_elite_market_fit(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int32_t market_score,
    int grade_rank)
{
    if (candidate == NULL) {
        return 0;
    }
    if (candidate->age >= 39u) {
        return (grade_rank >= 2 && market_score >= 105000)
            || candidate->score >= 125000
            || current_score >= 112000;
    }
    if (candidate->age >= 37u) {
        return (grade_rank >= 2 && market_score >= 98000)
            || candidate->score >= 115000
            || current_score >= 105000;
    }
    if (candidate->age >= 34u) {
        return (grade_rank >= 2 && market_score >= 78000)
            || market_score >= 98000
            || current_score >= 96000;
    }
    return grade_rank >= 2
        || market_score >= 76000
        || current_score >= 72000
        || upside_score >= 82000;
}

int kbo_fa_declaration_should_retry_after_down_year(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int32_t form_gap,
    int grade_rank)
{
    if (candidate == NULL) {
        return 0;
    }
    int established_or_paid = grade_rank >= 2
        || candidate->score >= 65000
        || upside_score >= 76000
        || candidate->salary >= 250000000;
    return established_or_paid
        && candidate->age <= 34u
        && current_score <= 62000
        && form_gap >= 14000
        && candidate->score < 92000;
}

int kbo_fa_declaration_should_stay_no_market(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int grade_rank,
    int elite_market_fit)
{
    if (candidate == NULL || elite_market_fit) {
        return 0;
    }

    int fringe_player = grade_rank <= 1
        && candidate->score < 64000
        && current_score < 58000
        && upside_score < 72000;
    int expensive_aging_risk = grade_rank <= 1
        && candidate->age >= 34u
        && candidate->salary >= 300000000
        && current_score < 62000;
    int weak_low_salary_market = grade_rank == 0
        && candidate->salary < 180000000
        && candidate->score < 60000
        && current_score < 56000;
    return fringe_player || expensive_aging_risk || weak_low_salary_market;
}

