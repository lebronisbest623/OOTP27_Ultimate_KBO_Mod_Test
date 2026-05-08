static int kbo_intl_established_fa_quality_shaping_enabled(void)
{
    return kbo_get_foreign_fa_quality_cap_enabled_setting();
}

static int kbo_intl_established_fa_pitcher_role_is_starter(uint8_t position_role)
{
    return position_role == 11u;
}

static int kbo_intl_established_fa_pitcher_role_is_bullpen(uint8_t position_role)
{
    return position_role == 12u || position_role == 13u;
}

static int kbo_intl_established_fa_position_is_catcher(uint8_t position_group, uint8_t position_role)
{
    return position_group == 2u || (position_group != 1u && position_role == 2u);
}

static const char* kbo_intl_established_fa_quality_policy_label(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    if (asian_quota) {
        if (position_group == 1u) {
            if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                return "asian_starter_cap";
            }
            if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                return "asian_bullpen_cap";
            }
            return "asian_pitcher_cap";
        }
        if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
            return "asian_catcher_cap";
        }
        return "asian_hitter_cap";
    }

    if (position_group == 1u) {
        if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
            return "non_asian_starter_cap";
        }
        if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
            return "non_asian_bullpen_cap";
        }
        return "non_asian_pitcher_cap";
    }
    if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
        return "non_asian_catcher_cap";
    }
    return "non_asian_hitter_cap";
}

static int32_t kbo_intl_established_fa_quality_score_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    if (asian_quota) {
        if (position_group == 1u) {
            if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                return KBO_INTL_ESTABLISHED_FA_ASIAN_STARTER_SCORE_CAP;
            }
            if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                return KBO_INTL_ESTABLISHED_FA_ASIAN_BULLPEN_SCORE_CAP;
            }
            return KBO_INTL_ESTABLISHED_FA_ASIAN_UNKNOWN_PITCHER_SCORE_CAP;
        }
        if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
            return KBO_INTL_ESTABLISHED_FA_ASIAN_CATCHER_SCORE_CAP;
        }
        return KBO_INTL_ESTABLISHED_FA_ASIAN_HITTER_SCORE_CAP;
    }

    if (position_group == 1u) {
        if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
            return KBO_INTL_ESTABLISHED_FA_NON_ASIAN_STARTER_SCORE_CAP;
        }
        if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
            return KBO_INTL_ESTABLISHED_FA_NON_ASIAN_BULLPEN_SCORE_CAP;
        }
        return KBO_INTL_ESTABLISHED_FA_NON_ASIAN_UNKNOWN_PITCHER_SCORE_CAP;
    }
    if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
        return KBO_INTL_ESTABLISHED_FA_NON_ASIAN_CATCHER_SCORE_CAP;
    }
    return KBO_INTL_ESTABLISHED_FA_NON_ASIAN_HITTER_SCORE_CAP;
}

static int16_t kbo_intl_established_fa_quality_field_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    int32_t score_cap = kbo_intl_established_fa_quality_score_cap(asian_quota, position_group, position_role);
    if (score_cap <= 0) {
        return 0;
    }
    return (int16_t)(score_cap / 100);
}

static int16_t kbo_intl_established_fa_scale_value_i16(int16_t value, int32_t original_score, int32_t cap)
{
    if (value <= 0 || original_score <= 0 || cap <= 0 || original_score <= cap) {
        return value;
    }

    int64_t scaled = ((int64_t)value * (int64_t)cap) / (int64_t)original_score;
    if (scaled < 1) {
        scaled = 1;
    }
    if (scaled > value) {
        scaled = value;
    }
    if (scaled > 32767) {
        scaled = 32767;
    }
    return (int16_t)scaled;
}

static int16_t kbo_intl_established_fa_clamp_value_i16(int16_t value, int16_t cap)
{
    if (cap <= 0 || value <= cap) {
        return value;
    }
    return cap;
}

static int kbo_intl_established_fa_apply_quality_cap(
    uint8_t* player,
    int32_t cap,
    int16_t field_cap,
    int32_t original_score,
    int32_t* out_adjusted_score)
{
    if (out_adjusted_score != NULL) {
        *out_adjusted_score = original_score;
    }
    if (player == NULL || cap <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int16_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    int needs_score_scale = original_score > cap;
    int needs_field_clamp = field_cap > 0
        && (overall > field_cap || talent > field_cap || ratings > field_cap || career > field_cap);
    if (!needs_score_scale && !needs_field_clamp) {
        return 0;
    }

    int16_t adjusted_overall = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(overall, original_score, cap)
        : overall;
    int16_t adjusted_talent = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(talent, original_score, cap)
        : talent;
    int16_t adjusted_ratings = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(ratings, original_score, cap)
        : ratings;
    int16_t adjusted_career = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(career, original_score, cap)
        : career;

    adjusted_overall = kbo_intl_established_fa_clamp_value_i16(adjusted_overall, field_cap);
    adjusted_talent = kbo_intl_established_fa_clamp_value_i16(adjusted_talent, field_cap);
    adjusted_ratings = kbo_intl_established_fa_clamp_value_i16(adjusted_ratings, field_cap);
    adjusted_career = kbo_intl_established_fa_clamp_value_i16(adjusted_career, field_cap);

    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) = adjusted_overall;
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = adjusted_talent;
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) = adjusted_ratings;
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET) = adjusted_career;

    int32_t adjusted_score = kbo_foreign_waiver_value_score(player);
    if (out_adjusted_score != NULL) {
        *out_adjusted_score = adjusted_score;
    }
    return adjusted_score < original_score;
}

