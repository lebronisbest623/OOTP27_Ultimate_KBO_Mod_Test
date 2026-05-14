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
