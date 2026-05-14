#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../fa_declaration_internal.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../fa_filing/fa_filing.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/names/team_name_cache.h"

int kbo_fa_declaration_case_candidate(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_DEFERRED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0;
}

int kbo_fa_declaration_find_candidate(
    const KboFaDeclarationCandidate* candidates,
    int count,
    uint32_t player_id)
{
    if (candidates == NULL || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].player_id == player_id) {
            return i;
        }
    }
    return -1;
}

uint32_t kbo_fa_declaration_team_league(uint32_t team_id, uint32_t fallback_league_id)
{
    if (team_id == 0u) {
        return fallback_league_id;
    }
    uint32_t league_id = kbo_fa_filing_team_league_id(team_id);
    return league_id != 0u ? league_id : fallback_league_id;
}

int32_t kbo_fa_declaration_contract_salary_for_season(
    uint8_t* player,
    uint32_t season,
    int32_t* out_next_salary)
{
    if (out_next_salary != NULL) {
        *out_next_salary = 0;
    }
    if (player == NULL
            || season == 0u
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(int32_t))
            || !memory_range_readable(
                player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
                OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        return 0;
    }

    int32_t years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS] = {0};
    for (uint32_t i = 0u; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }

    int32_t start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    uint32_t index = 0u;
    int have_index = 0;
    if (start_year > 0 && season >= (uint32_t)start_year) {
        uint32_t candidate_index = season - (uint32_t)start_year;
        if (candidate_index < OOTP27_PLAYER_CONTRACT_SALARY_YEARS) {
            index = candidate_index;
            have_index = 1;
        }
    }
    if (!have_index) {
        index = 0u;
    }

    int32_t current_salary = years[index];
    int32_t next_salary = 0;
    if (index + 1u < OOTP27_PLAYER_CONTRACT_SALARY_YEARS) {
        next_salary = years[index + 1u];
    }
    if (out_next_salary != NULL) {
        *out_next_salary = next_salary;
    }
    if (current_salary <= 0) {
        for (uint32_t i = 0u; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
            if (years[i] > 0) {
                current_salary = years[i];
                break;
            }
        }
    }
    return current_salary;
}

void kbo_fa_declaration_fill_from_player(
    KboFaDeclarationCandidate* candidate,
    uint8_t* player,
    uint32_t season)
{
    if (candidate == NULL || player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    candidate->player_ptr = (uintptr_t)player;
    candidate->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    candidate->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    candidate->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    candidate->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    candidate->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    candidate->retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
    candidate->fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    candidate->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    candidate->talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    candidate->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    candidate->career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    candidate->score = kbo_foreign_waiver_value_score(player);
    if (candidate->salary <= 0) {
        candidate->salary = kbo_fa_declaration_contract_salary_for_season(player, season, NULL);
    }
    kbo_copy_player_display_name(player, candidate->player_name, sizeof(candidate->player_name));
    if (candidate->player_name[0] == '\0' || strcmp(candidate->player_name, "Unknown player") == 0) {
        snprintf(candidate->player_name, sizeof(candidate->player_name), "Player #%u", candidate->player_id);
    }
}

void kbo_fa_declaration_apply_salary_grade(
    KboFaDeclarationCandidate* candidate,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count)
{
    if (candidate == NULL || candidate->player_id == 0u || grades == NULL || grade_count <= 0) {
        return;
    }
    const KboFaSalarySnapshotGrade* grade = kbo_find_fa_salary_snapshot_grade(
        grades,
        grade_count,
        candidate->player_id);
    if (grade == NULL) {
        return;
    }
    if (candidate->grade[0] == '\0' || strcmp(candidate->grade, "UNKNOWN") == 0) {
        snprintf(candidate->grade, sizeof(candidate->grade), "%s", grade->grade);
    }
    if (candidate->salary <= 0) {
        candidate->salary = grade->salary;
    }
    if (candidate->team_id == 0u) {
        candidate->team_id = grade->ranking_team_id;
    }
    if (candidate->player_name[0] == '\0' && grade->player_name[0] != '\0') {
        snprintf(candidate->player_name, sizeof(candidate->player_name), "%s", grade->player_name);
    }
}

static int32_t kbo_fa_declaration_nonnegative_i16(int16_t value)
{
    return value > 0 ? (int32_t)value : 0;
}

static int32_t kbo_fa_declaration_current_market_score(const KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return 0;
    }
    int32_t overall = kbo_fa_declaration_nonnegative_i16(candidate->overall);
    int32_t ratings = kbo_fa_declaration_nonnegative_i16(candidate->ratings);
    int32_t career = kbo_fa_declaration_nonnegative_i16(candidate->career);
    return (overall * 45) + (ratings * 35) + (career * 20);
}

static int32_t kbo_fa_declaration_upside_score(const KboFaDeclarationCandidate* candidate)
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

static int kbo_fa_declaration_grade_rank(const char* grade)
{
    if (grade == NULL || grade[0] == '\0') {
        return 0;
    }
    if (_stricmp(grade, "A") == 0) { return 3; }
    if (_stricmp(grade, "B") == 0) { return 2; }
    if (_stricmp(grade, "C") == 0) { return 1; }
    return 0;
}

static int kbo_fa_declaration_player_is_good(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int grade_rank)
{
    if (candidate == NULL) {
        return 0;
    }
    return grade_rank >= 2
        || candidate->score >= 72000
        || current_score >= 70000
        || upside_score >= 78000
        || (candidate->salary >= 300000000 && current_score >= 60000);
}

static int kbo_fa_declaration_should_retry_after_down_year(
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

static int kbo_fa_declaration_should_stay_no_market(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int grade_rank,
    int player_is_good)
{
    if (candidate == NULL || player_is_good) {
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

void kbo_fa_declaration_decide(KboFaDeclarationCandidate* candidate)
{
    if (candidate == NULL) {
        return;
    }

    if (strcmp(candidate->case_label, "KBO_FA_APPROVED") == 0) {
        candidate->declared = 1u;
        candidate->threshold = 0;
        snprintf(candidate->decision_reason, sizeof(candidate->decision_reason), "manual approved FA seed");
        return;
    }
    if (strcmp(candidate->case_label, "KBO_FA_DEFERRED") == 0) {
        candidate->declared = 0u;
        candidate->threshold = 2147483647;
        snprintf(candidate->decision_reason, sizeof(candidate->decision_reason), "manual deferred FA seed");
        return;
    }
    if (strcmp(candidate->case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) {
        candidate->declared = 0u;
        candidate->threshold = 2147483647;
        snprintf(candidate->decision_reason, sizeof(candidate->decision_reason), "manual eligible-not-approved FA seed");
        return;
    }

    int grade_rank = kbo_fa_declaration_grade_rank(candidate->grade);
    int32_t current_score = kbo_fa_declaration_current_market_score(candidate);
    int32_t upside_score = kbo_fa_declaration_upside_score(candidate);
    int32_t form_gap = upside_score > current_score ? upside_score - current_score : 0;

    int32_t threshold = 70000;
    if (grade_rank >= 3) {
        threshold = 50000;
    } else if (grade_rank == 2) {
        threshold = 56000;
    } else if (grade_rank == 1) {
        threshold = 64000;
    }

    if (candidate->age >= 37u) {
        threshold += 20000;
    } else if (candidate->age >= 34u) {
        threshold += 8000;
    } else if (candidate->age >= 29u && candidate->age <= 31u) {
        threshold -= 4000;
    }

    if (candidate->salary >= 700000000) {
        threshold += 8000;
    } else if (candidate->salary > 0 && candidate->salary <= 120000000) {
        threshold -= 3000;
    }

    if (strcmp(candidate->case_label, "KBO_FA_ELIGIBLE_PROXY") == 0) {
        threshold += 8000;
    }

    candidate->threshold = threshold;

    int player_is_good = kbo_fa_declaration_player_is_good(
        candidate,
        current_score,
        upside_score,
        grade_rank);
    if (kbo_fa_declaration_should_retry_after_down_year(
            candidate,
            current_score,
            upside_score,
            form_gap,
            grade_rank)) {
        candidate->declared = 0u;
        snprintf(
            candidate->decision_reason,
            sizeof(candidate->decision_reason),
            "ai_deferred_retry_after_down_year current=%d upside=%d gap=%d score=%d threshold=%d age=%u grade=%s salary=%d",
            current_score,
            upside_score,
            form_gap,
            candidate->score,
            candidate->threshold,
            (uint32_t)candidate->age,
            candidate->grade[0] != '\0' ? candidate->grade : "UNKNOWN",
            candidate->salary);
        return;
    }

    if (kbo_fa_declaration_should_stay_no_market(
            candidate,
            current_score,
            upside_score,
            grade_rank,
            player_is_good)) {
        candidate->declared = 0u;
        snprintf(
            candidate->decision_reason,
            sizeof(candidate->decision_reason),
            "ai_deferred_no_market_stay_original current=%d upside=%d score=%d threshold=%d age=%u grade=%s salary=%d",
            current_score,
            upside_score,
            candidate->score,
            candidate->threshold,
            (uint32_t)candidate->age,
            candidate->grade[0] != '\0' ? candidate->grade : "UNKNOWN",
            candidate->salary);
        return;
    }

    candidate->declared = (player_is_good || candidate->score >= threshold || current_score >= threshold) ? 1u : 0u;
    snprintf(
        candidate->decision_reason,
        sizeof(candidate->decision_reason),
        "%s current=%d upside=%d gap=%d score=%d threshold=%d age=%u grade=%s salary=%d",
        candidate->declared ? "ai_declared_good_or_market_fit" : "ai_deferred_no_market_stay_original",
        current_score,
        upside_score,
        form_gap,
        candidate->score,
        candidate->threshold,
        (uint32_t)candidate->age,
        candidate->grade[0] != '\0' ? candidate->grade : "UNKNOWN",
        candidate->salary);
}
