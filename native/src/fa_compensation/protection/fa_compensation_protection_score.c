#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "fa_compensation_protection_score.h"
#include "policy/fa_compensation_protection_policy.h"

static int kbo_fa_role_bucket(uint8_t role);
static int kbo_fa_team_role_count(uint32_t team_id, int role_bucket);

static int kbo_fa_player_vector_readable(uintptr_t player_vector, int32_t player_count)
{
    return player_vector != 0 && player_count > 0 && player_count <= 200000
        && (SIZE_T)player_count <= ((SIZE_T)-1 / sizeof(uintptr_t))
        && memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t));
}

static int kbo_fa_protected_candidate_compare_desc(const void* left, const void* right)
{
    const KboFaProtectedCandidate* a = (const KboFaProtectedCandidate*)left;
    const KboFaProtectedCandidate* b = (const KboFaProtectedCandidate*)right;
    if (a->score != b->score) {
        return a->score > b->score ? -1 : 1;
    }
    if (a->age != b->age) {
        return a->age < b->age ? -1 : 1;
    }
    if (a->player_id == b->player_id) {
        return 0;
    }
    return a->player_id < b->player_id ? -1 : 1;
}

static int32_t kbo_fa_protection_candidate_score(
    uint8_t* player,
    uint32_t signing_team_id,
    char* reason,
    size_t reason_size)
{
    if (reason != NULL && reason_size > 0) {
        reason[0] = '\0';
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -2147483647;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    int role_bucket = kbo_fa_role_bucket(role);
    int team_role_count = kbo_fa_team_role_count(signing_team_id, role_bucket);
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();

    int32_t score = base;
    const char* age_tag = "prime";
    if ((int32_t)age <= policy->candidate_prospect_age_max) {
        score += policy->candidate_prospect_bonus;
        age_tag = "prospect";
    } else if ((int32_t)age <= policy->candidate_young_age_max) {
        score += policy->candidate_young_bonus;
        age_tag = "young";
    } else if ((int32_t)age <= policy->candidate_prime_age_max) {
        score += policy->candidate_prime_bonus;
        age_tag = "prime";
    } else if ((int32_t)age >= policy->candidate_old_low_overall_age_min
            && overall <= policy->candidate_old_low_overall_max) {
        score -= policy->candidate_old_low_overall_penalty;
        age_tag = "old_low_ovr";
    } else if ((int32_t)age >= policy->candidate_aging_age_min) {
        score -= policy->candidate_aging_penalty;
        age_tag = "aging";
    }

    const char* core_tag = "depth";
    if (overall >= policy->candidate_core_value_min || career >= policy->candidate_core_value_min) {
        score += policy->candidate_core_bonus;
        core_tag = "core";
    } else if (overall >= policy->candidate_regular_value_min || career >= policy->candidate_regular_value_min) {
        score += policy->candidate_regular_bonus;
        core_tag = "regular";
    }
    if (talent > overall + policy->candidate_upside_margin) {
        score += policy->candidate_upside_bonus;
        core_tag = "upside";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        salary_tag = "unknown_salary";
    } else if (salary <= policy->candidate_cheap_salary_max
            && ((int32_t)age <= policy->candidate_cheap_age_max
                || talent >= overall + policy->candidate_cheap_talent_margin)) {
        score += policy->candidate_cheap_bonus;
        salary_tag = "cheap_control";
    } else if (salary >= policy->candidate_bad_contract_salary_min
            && (int32_t)age >= policy->candidate_bad_contract_age_min
            && overall <= policy->candidate_bad_contract_overall_max) {
        score -= policy->candidate_bad_contract_penalty;
        salary_tag = "bad_contract";
    } else if (salary >= policy->candidate_costly_vet_salary_min
            && (int32_t)age >= policy->candidate_costly_vet_age_min) {
        score -= policy->candidate_costly_vet_penalty;
        salary_tag = "costly_vet";
    } else if (salary >= policy->candidate_paid_core_salary_min
            && overall >= policy->candidate_paid_core_overall_min) {
        score += policy->candidate_paid_core_bonus;
        salary_tag = "paid_core";
    }

    const char* scarcity_tag = "normal_role";
    if ((role_bucket == 1 && team_role_count <= policy->candidate_scarce_catcher_max)
            || (role_bucket == 2 && team_role_count <= policy->candidate_scarce_infielder_max)
            || (role_bucket == 3 && team_role_count <= policy->candidate_scarce_outfielder_max)
            || (role_bucket == 0 && team_role_count <= policy->candidate_scarce_pitcher_max)) {
        score += policy->candidate_scarce_bonus;
        scarcity_tag = "scarce_role";
    } else if ((role_bucket == 1 && team_role_count >= policy->candidate_deep_catcher_min)
            || (role_bucket == 2 && team_role_count >= policy->candidate_deep_infielder_min)
            || (role_bucket == 3 && team_role_count >= policy->candidate_deep_outfielder_min)
            || (role_bucket == 0 && team_role_count >= policy->candidate_deep_pitcher_min)) {
        score -= policy->candidate_deep_penalty;
        scarcity_tag = "deep_role";
    }

    if (reason != NULL && reason_size > 0) {
        snprintf(
            reason,
            reason_size,
            "%s/%s/%s/%s/base%d",
            age_tag,
            core_tag,
            salary_tag,
            scarcity_tag,
            base);
    }
    return score;
}
static int kbo_fa_role_bucket(uint8_t role)
{
    if (role == 1u) {
        return 0; /* Pitcher */
    }
    if (role == 2u) {
        return 1; /* Catcher */
    }
    if (role >= 3u && role <= 6u) {
        return 2; /* Infielder */
    }
    if (role >= 7u && role <= 9u) {
        return 3; /* Outfielder */
    }
    return 4;
}

static int kbo_fa_team_role_count(uint32_t team_id, int role_bucket)
{
    if (team_id == 0u || role_bucket < 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || !kbo_fa_player_vector_readable(player_vector, player_count)) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t slot = player_vector + ((uintptr_t)i * sizeof(uintptr_t));
        if (!memory_range_readable((void*)slot, sizeof(uintptr_t))) { break; }
        uintptr_t player_ptr = *(uintptr_t*)slot;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u) {
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (current_team_id != team_id && active_team_id != team_id) {
            continue;
        }
        if (kbo_fa_role_bucket(player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]) == role_bucket) {
            count++;
        }
    }
    return count;
}

int32_t kbo_fa_compensation_player_decision_score(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidate,
    char* reason,
    size_t reason_size)
{
    if (reason != NULL && reason_size > 0) {
        reason[0] = '\0';
    }
    if (rec == NULL || candidate == NULL || candidate->player_id == 0u) {
        return -2147483647;
    }

    uint8_t* player = kbo_find_player_by_id(candidate->player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();
        return candidate->score - policy->decision_missing_player_penalty;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int role_bucket = kbo_fa_role_bucket(candidate->role);
    int team_role_count = kbo_fa_team_role_count(rec->original_team_id, role_bucket);
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();

    int32_t score = base;
    const char* age_tag = "prime";
    if ((int32_t)candidate->age <= policy->decision_prospect_age_max) {
        score += policy->decision_prospect_bonus;
        age_tag = "prospect";
    } else if ((int32_t)candidate->age <= policy->decision_young_age_max) {
        score += policy->decision_young_bonus;
        age_tag = "young";
    } else if ((int32_t)candidate->age <= policy->decision_prime_age_max) {
        score += policy->decision_prime_bonus;
        age_tag = "prime";
    } else if ((int32_t)candidate->age >= policy->decision_old_age_min) {
        score -= policy->decision_old_penalty;
        age_tag = "old";
    } else if ((int32_t)candidate->age >= policy->decision_aging_age_min) {
        score -= policy->decision_aging_penalty;
        age_tag = "aging";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        score += policy->decision_unknown_salary_bonus;
        salary_tag = "unknown_salary";
    } else if (salary <= policy->decision_cheap_salary_max) {
        score += policy->decision_cheap_bonus;
        salary_tag = "cheap";
    } else if (salary >= policy->decision_expensive_salary_min) {
        score -= policy->decision_expensive_penalty;
        salary_tag = "expensive";
    } else if (salary >= policy->decision_costly_salary_min) {
        score -= policy->decision_costly_penalty;
        salary_tag = "costly";
    }

    const char* need_tag = "neutral_need";
    if ((role_bucket == 1 && team_role_count < policy->decision_need_catcher_below)
            || (role_bucket == 2 && team_role_count < policy->decision_need_infielder_below)
            || (role_bucket == 3 && team_role_count < policy->decision_need_outfielder_below)
            || (role_bucket == 0 && team_role_count < policy->decision_need_pitcher_below)) {
        score += policy->decision_team_need_bonus;
        need_tag = "team_need";
    } else if ((role_bucket == 1 && team_role_count > policy->decision_surplus_catcher_above)
            || (role_bucket == 2 && team_role_count > policy->decision_surplus_infielder_above)
            || (role_bucket == 3 && team_role_count > policy->decision_surplus_outfielder_above)
            || (role_bucket == 0 && team_role_count > policy->decision_surplus_pitcher_above)) {
        score -= policy->decision_surplus_penalty;
        need_tag = "surplus_role";
    }

    if (talent > overall + policy->decision_upside_margin) {
        score += policy->decision_upside_bonus;
    }

    if (reason != NULL && reason_size > 0) {
        snprintf(
            reason,
            reason_size,
            "%s/%s/%s/base%d",
            age_tag,
            salary_tag,
            need_tag,
            base);
    }
    return score;
}

static int kbo_fa_compensation_player_is_rookie_auto_protected(
    uint8_t* player,
    const KboFaCompensationRecord* rec)
{
    if (player == NULL || rec == NULL) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();
    if ((int32_t)age > policy->rookie_auto_protected_age_max) {
        return 0;
    }
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (draft_league_id != rec->league_id) {
        return 0;
    }
    uint8_t draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
    uint8_t draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
    uint8_t draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
    uint8_t generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    return draft_class != 0u || draft_subtype != 0u || draft_extra != 0u || generation_context != 0u;
}

int kbo_build_fa_compensation_protected_candidates(
    const KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected)
{
    if (rec == NULL || candidates == NULL || max_candidates <= 0
            || rec->signing_team_id == 0u || rec->protect_count == 0u) {
        return 0;
    }
    memset(candidates, 0, (SIZE_T)max_candidates * sizeof(candidates[0]));
    if (auto_protected != NULL && max_auto_protected > 0) {
        memset(auto_protected, 0, (SIZE_T)max_auto_protected * sizeof(auto_protected[0]));
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || !kbo_fa_player_vector_readable(player_vector, player_count)) {
        return 0;
    }

    int count = 0;
    int auto_count = 0;
    for (int32_t i = 0; i < player_count && count < max_candidates; i++) {
        uintptr_t slot = player_vector + ((uintptr_t)i * sizeof(uintptr_t));
        if (!memory_range_readable((void*)slot, sizeof(uintptr_t))) { break; }
        uintptr_t player_ptr = *(uintptr_t*)slot;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (player_id == 0u || (current_team_id != rec->signing_team_id && active_team_id != rec->signing_team_id)) {
            continue;
        }
        if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
            continue;
        }

        const char* auto_reason = NULL;
        if (player_id == rec->player_id) {
            auto_reason = "signed_fa";
        } else if (kbo_player_is_foreign_for_kbo_rights(player)) {
            auto_reason = "foreign";
        } else if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u) {
            auto_reason = "military";
        } else if (kbo_fa_compensation_player_is_rookie_auto_protected(player, rec)) {
            auto_reason = "rookie_draftee";
        }
        if (auto_reason != NULL) {
            if (auto_protected != NULL && auto_count < max_auto_protected) {
                KboFaProtectedCandidate* automatic = &auto_protected[auto_count++];
                automatic->player_id = player_id;
                automatic->team_id = current_team_id != 0u ? current_team_id : active_team_id;
                automatic->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
                automatic->role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
                automatic->score = kbo_fa_protection_candidate_score(player, rec->signing_team_id, automatic->reason, sizeof(automatic->reason));
                snprintf(automatic->reason, sizeof(automatic->reason), "%s", auto_reason);
                kbo_copy_player_display_name(player, automatic->player_name, sizeof(automatic->player_name));
                if (automatic->player_name[0] == '\0' || strcmp(automatic->player_name, "Unknown player") == 0) {
                    snprintf(automatic->player_name, sizeof(automatic->player_name), "Player #%u", player_id);
                }
            }
            continue;
        }

        KboFaProtectedCandidate* candidate = &candidates[count++];
        candidate->player_id = player_id;
        candidate->team_id = current_team_id != 0u ? current_team_id : active_team_id;
        candidate->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        candidate->role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
        candidate->score = kbo_fa_protection_candidate_score(player, rec->signing_team_id, candidate->reason, sizeof(candidate->reason));
        kbo_copy_player_display_name(player, candidate->player_name, sizeof(candidate->player_name));
        if (candidate->player_name[0] == '\0' || strcmp(candidate->player_name, "Unknown player") == 0) {
            snprintf(candidate->player_name, sizeof(candidate->player_name), "Player #%u", player_id);
        }
    }

    qsort(candidates, (size_t)count, sizeof(candidates[0]), kbo_fa_protected_candidate_compare_desc);
    return count;
}
