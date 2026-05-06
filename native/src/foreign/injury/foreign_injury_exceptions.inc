/* Foreign injury replacement roster/signing exception policy. Included from foreign_injury_replacement.inc. */

static int kbo_foreign_injury_replacement_signing_exception_available(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_effective_count,
    uint32_t* out_effective_limit)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_effective_count != NULL) { *out_effective_count = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = 3u; }

    if (!kbo_foreign_injury_replacement_enabled()
            || team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    if (out_effective_count != NULL) { *out_effective_count = effective; }
    if (out_effective_limit != NULL) { *out_effective_limit = 4u; }

    int candidate_asian = kbo_player_is_asian_quota_candidate(candidate);
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    uint32_t injured_player_id = 0u;
    if (candidate_asian
            && asian_count <= 1u
            && kbo_team_has_foreign_injury_slot_for_candidate(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                candidate_id,
                &injured_player_id,
                NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1;
    }

    if (effective <= 3u
            && kbo_team_has_foreign_injury_slot_for_candidate(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_REGULAR,
                candidate_id,
                &injured_player_id,
                NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1;
    }

    return 0;
}

static int kbo_foreign_injury_replacement_callup_exception_available(
    uintptr_t team_ptr,
    uint8_t* candidate,
    int32_t limit,
    int check_type,
    uint32_t* out_effective_after,
    uint8_t* out_slot_type)
{
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (!kbo_foreign_injury_replacement_enabled()
            || team_ptr == 0
            || limit <= 0
            || candidate == NULL
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    if (team_id == 0u) {
        return 0;
    }

    int candidate_pitcher = (*(uint8_t*)(candidate + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    int candidate_asian = kbo_player_is_asian_quota_candidate(candidate);
    uint8_t slot_type = candidate_asian ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA : KBO_FOREIGN_INJURY_SLOT_REGULAR;
    uint32_t injured_player_id = 0u;
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (!kbo_team_has_foreign_injury_slot_for_candidate(team_id, slot_type, candidate_id, &injured_player_id, NULL)) {
        if (slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                && !kbo_team_has_foreign_injury_slot_for_candidate(
                    team_id,
                    KBO_FOREIGN_INJURY_SLOT_REGULAR,
                    candidate_id,
                    &injured_player_id,
                    NULL)) {
            return 0;
        }
        slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    if (check_type == 1 && candidate_pitcher) {
        return 0;
    }
    if (check_type == 2 && !candidate_pitcher) {
        return 0;
    }

    uint32_t asian_after = asian_hitters + asian_pitchers;
    uint32_t non_asian_after = non_asian_hitters + non_asian_pitchers;
    if (check_type == 1) {
        asian_after = asian_hitters + (!candidate_pitcher && candidate_asian ? 1u : 0u);
        non_asian_after = non_asian_hitters + (!candidate_pitcher && !candidate_asian ? 1u : 0u);
    } else if (check_type == 2) {
        asian_after = asian_pitchers + (candidate_pitcher && candidate_asian ? 1u : 0u);
        non_asian_after = non_asian_pitchers + (candidate_pitcher && !candidate_asian ? 1u : 0u);
    } else {
        asian_after += candidate_asian ? 1u : 0u;
        non_asian_after += candidate_asian ? 0u : 1u;
    }

    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    return effective_after <= ((uint32_t)limit + 1u);
}
