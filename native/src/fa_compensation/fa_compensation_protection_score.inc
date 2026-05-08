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

    int32_t score = base;
    const char* age_tag = "prime";
    if (age <= 22u) {
        score += 1200;
        age_tag = "prospect";
    } else if (age <= 25u) {
        score += 850;
        age_tag = "young";
    } else if (age <= 29u) {
        score += 350;
        age_tag = "prime";
    } else if (age >= 36u && overall < 45) {
        score -= 900;
        age_tag = "old_low_ovr";
    } else if (age >= 34u) {
        score -= 350;
        age_tag = "aging";
    }

    const char* core_tag = "depth";
    if (overall >= 60 || career >= 60) {
        score += 1000;
        core_tag = "core";
    } else if (overall >= 50 || career >= 50) {
        score += 450;
        core_tag = "regular";
    }
    if (talent > overall + 15) {
        score += 650;
        core_tag = "upside";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        salary_tag = "unknown_salary";
    } else if (salary <= 70000000 && (age <= 27u || talent >= overall + 10)) {
        score += 450;
        salary_tag = "cheap_control";
    } else if (salary >= 800000000 && age >= 33u && overall < 55) {
        score -= 1000;
        salary_tag = "bad_contract";
    } else if (salary >= 500000000 && age >= 32u) {
        score -= 450;
        salary_tag = "costly_vet";
    } else if (salary >= 300000000 && overall >= 60) {
        score += 200;
        salary_tag = "paid_core";
    }

    const char* scarcity_tag = "normal_role";
    if ((role_bucket == 1 && team_role_count <= 2)
            || (role_bucket == 2 && team_role_count <= 5)
            || (role_bucket == 3 && team_role_count <= 6)
            || (role_bucket == 0 && team_role_count <= 12)) {
        score += 650;
        scarcity_tag = "scarce_role";
    } else if ((role_bucket == 1 && team_role_count >= 5)
            || (role_bucket == 2 && team_role_count >= 11)
            || (role_bucket == 3 && team_role_count >= 11)
            || (role_bucket == 0 && team_role_count >= 24)) {
        score -= 200;
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
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
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

static int32_t kbo_fa_compensation_player_decision_score(
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
        return candidate->score - 100000;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int role_bucket = kbo_fa_role_bucket(candidate->role);
    int team_role_count = kbo_fa_team_role_count(rec->original_team_id, role_bucket);

    int32_t score = base;
    const char* age_tag = "prime";
    if (candidate->age <= 22u) {
        score += 1000;
        age_tag = "prospect";
    } else if (candidate->age <= 25u) {
        score += 700;
        age_tag = "young";
    } else if (candidate->age <= 29u) {
        score += 250;
        age_tag = "prime";
    } else if (candidate->age >= 35u) {
        score -= 900;
        age_tag = "old";
    } else if (candidate->age >= 32u) {
        score -= 350;
        age_tag = "aging";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        score += 150;
        salary_tag = "unknown_salary";
    } else if (salary <= 70000000) {
        score += 500;
        salary_tag = "cheap";
    } else if (salary >= 600000000) {
        score -= 900;
        salary_tag = "expensive";
    } else if (salary >= 300000000) {
        score -= 350;
        salary_tag = "costly";
    }

    const char* need_tag = "neutral_need";
    if ((role_bucket == 1 && team_role_count < 2)
            || (role_bucket == 2 && team_role_count < 5)
            || (role_bucket == 3 && team_role_count < 7)
            || (role_bucket == 0 && team_role_count < 13)) {
        score += 700;
        need_tag = "team_need";
    } else if ((role_bucket == 1 && team_role_count > 4)
            || (role_bucket == 2 && team_role_count > 10)
            || (role_bucket == 3 && team_role_count > 10)
            || (role_bucket == 0 && team_role_count > 22)) {
        score -= 250;
        need_tag = "surplus_role";
    }

    if (talent > overall + 15) {
        score += 450;
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
    if (age > 23u) {
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

static int kbo_build_fa_compensation_protected_candidates(
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
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    int auto_count = 0;
    for (int32_t i = 0; i < player_count && count < max_candidates; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
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
