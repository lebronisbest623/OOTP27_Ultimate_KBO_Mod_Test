/* Post-registration scan for international established free agents. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS 4500ull
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS 2500ull
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES 8
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_DETAIL_LOGS 160
#define KBO_INTL_ESTABLISHED_FA_ASIAN_STARTER_SCORE_CAP 72000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_BULLPEN_SCORE_CAP 70000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_UNKNOWN_PITCHER_SCORE_CAP 71000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_HITTER_SCORE_CAP 72000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_CATCHER_SCORE_CAP 65000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_STARTER_SCORE_CAP 115000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_BULLPEN_SCORE_CAP 95000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_UNKNOWN_PITCHER_SCORE_CAP 105000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_HITTER_SCORE_CAP 110000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_CATCHER_SCORE_CAP 80000

typedef struct KboIntlEstablishedFaPostscanState {
    LONG pending;
    LONG batch_id;
    int32_t before_count;
    uint32_t before_max_player_id;
    int32_t original_count;
    int32_t expected_count;
    int multiplier;
    uint32_t primary_league_id;
    uint32_t fallback_league_id;
    uint32_t scheduled_date;
    ULONGLONG due_tick;
    int attempts;
} KboIntlEstablishedFaPostscanState;

static KboIntlEstablishedFaPostscanState g_kbo_intl_established_fa_postscan = {0};
static volatile LONG g_kbo_intl_established_fa_postscan_worker_started = 0;

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

static int kbo_intl_established_fa_postscan_csv_empty(HANDLE file)
{
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size)) {
        return 0;
    }
    return size.QuadPart == 0;
}

static int kbo_intl_established_fa_postscan_open_csv(HANDLE* out_file, char* out_path, size_t out_path_size)
{
    if (out_file == NULL || out_path == NULL || out_path_size == 0) {
        return 0;
    }
    *out_file = INVALID_HANDLE_VALUE;
    out_path[0] = '\0';

    if (!kbo_get_save_scoped_data_file("intl_established_fa_postscan.csv", out_path, out_path_size)) {
        return 0;
    }

    HANDLE file = CreateFileA(
        out_path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (kbo_intl_established_fa_postscan_csv_empty(file)) {
        const char* header =
            "date,batch_id,before_count,after_count,expected_count,index,player_id,nation_id,asian_quota,age,"
            "pos_group,pos_role,current_team_id,active_team_id,current_league_id,draft_league_id,"
            "generation_flags,generation_context,generation_grade,generation_special,"
            "overall,talent,ratings,career,value_score\r\n";
        DWORD written = 0;
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    *out_file = file;
    return 1;
}

static void kbo_intl_established_fa_postscan_write_csv_row(
    HANDLE file,
    const char* date,
    LONG batch_id,
    int32_t before_count,
    int32_t after_count,
    int32_t expected_count,
    int32_t index,
    uint8_t* player)
{
    if (file == INVALID_HANDLE_VALUE || player == NULL) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    int asian_quota = kbo_nation_is_asian_quota_candidate(nation_id);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
    uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
    uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
    uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int32_t value_score = kbo_foreign_waiver_value_score(player);

    char line[512] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%ld,%d,%d,%d,%d,%u,%u,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d\r\n",
        date != NULL ? date : "00000000",
        batch_id,
        before_count,
        after_count,
        expected_count,
        index,
        player_id,
        nation_id,
        asian_quota,
        (int)age,
        position_group,
        position_role,
        current_team_id,
        active_team_id,
        current_league_id,
        draft_league_id,
        generation_flags,
        generation_context,
        generation_grade,
        generation_special,
        overall,
        talent,
        ratings,
        career,
        value_score);
    if (len <= 0 || (size_t)len >= sizeof(line)) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static void kbo_intl_established_fa_postscan_schedule(
    int32_t original_count,
    int32_t expected_count,
    int multiplier,
    uint32_t primary_league_id,
    uint32_t fallback_league_id)
{
    if (expected_count <= 0) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t max_player_id = 0u;
    int vector_ready = find_kbo_global_player_vector(&player_vector, &player_count, NULL);
    if (vector_ready) {
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (player_id > max_player_id) {
                max_player_id = player_id;
            }
        }
    }

    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    LONG batch_id = InterlockedIncrement(&g_kbo_intl_established_fa_postscan.batch_id);
    g_kbo_intl_established_fa_postscan.before_count = vector_ready ? player_count : 0;
    g_kbo_intl_established_fa_postscan.before_max_player_id = max_player_id;
    g_kbo_intl_established_fa_postscan.original_count = original_count;
    g_kbo_intl_established_fa_postscan.expected_count = expected_count;
    g_kbo_intl_established_fa_postscan.multiplier = multiplier;
    g_kbo_intl_established_fa_postscan.primary_league_id = primary_league_id;
    g_kbo_intl_established_fa_postscan.fallback_league_id = fallback_league_id;
    g_kbo_intl_established_fa_postscan.scheduled_date = today;
    g_kbo_intl_established_fa_postscan.due_tick = GetTickCount64() + KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS;
    g_kbo_intl_established_fa_postscan.attempts = 0;
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);

    append_logf(
        "international established FA postscan scheduled batch=%ld league_id=%u/%u before_count=%d before_max_player=%u original=%d multiplier=%d expected=%d date=%u vector_ready=%d",
        batch_id,
        primary_league_id,
        fallback_league_id,
        vector_ready ? player_count : 0,
        max_player_id,
        original_count,
        multiplier,
        expected_count,
        today,
        vector_ready ? 1 : 0);
}

static int kbo_intl_established_fa_postscan_candidate_matches(
    const KboIntlEstablishedFaPostscanState* batch,
    int32_t index,
    int32_t player_count,
    uint8_t* player)
{
    if (batch == NULL || player == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (batch->before_max_player_id != 0u) {
        return player_id > batch->before_max_player_id;
    }

    if (batch->before_count > 0 && batch->before_count < player_count) {
        return index >= batch->before_count;
    }

    if (batch->expected_count > 0 && player_count > batch->expected_count) {
        int32_t fallback_start = player_count - batch->expected_count - 8;
        if (fallback_start < 0) {
            fallback_start = 0;
        }
        return index >= fallback_start;
    }

    return 1;
}

static void kbo_intl_established_fa_postscan_run(const KboIntlEstablishedFaPostscanState* batch)
{
    if (batch == NULL || !kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_logf(
            "international established FA postscan failed batch=%ld reason=no_player_vector expected=%d before_count=%d",
            batch->batch_id,
            batch->expected_count,
            batch->before_count);
        return;
    }

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, "intl_established_fa_postscan")) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    char csv_path[MAX_PATH] = {0};
    HANDLE csv_file = INVALID_HANDLE_VALUE;
    int csv_ok = kbo_intl_established_fa_postscan_open_csv(&csv_file, csv_path, sizeof(csv_path));

    int matched = 0;
    int valid = 0;
    int foreign = 0;
    int asian = 0;
    int non_asian = 0;
    int teamless = 0;
    int league_match = 0;
    int logged = 0;
    int quality_shaping_enabled = kbo_intl_established_fa_quality_shaping_enabled();
    int pitcher_count = 0;
    int asian_pitcher_count = 0;
    int non_asian_pitcher_count = 0;
    int asian_starter_pitcher_count = 0;
    int asian_bullpen_pitcher_count = 0;
    int non_asian_starter_pitcher_count = 0;
    int non_asian_bullpen_pitcher_count = 0;
    int quality_adjusted_count = 0;
    int asian_pitcher_adjusted_count = 0;
    int non_asian_pitcher_adjusted_count = 0;
    int starter_pitcher_adjusted_count = 0;
    int bullpen_pitcher_adjusted_count = 0;
    int64_t score_sum = 0;
    int64_t asian_score_sum = 0;
    int64_t non_asian_score_sum = 0;
    int32_t max_score = 0;
    int32_t asian_max_score = 0;
    int32_t non_asian_max_score = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_intl_established_fa_postscan_candidate_matches(batch, i, player_count, player)) {
            continue;
        }

        matched++;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        valid++;

        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        foreign++;

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        int is_asian = kbo_nation_is_asian_quota_candidate(nation_id);
        int32_t score = kbo_foreign_waiver_value_score(player);
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
        uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
        int32_t original_score = score;
        int32_t quality_cap = 0;
        int16_t quality_field_cap = 0;
        int was_quality_adjusted = 0;
        const char* quality_policy = "none";

        if (position_group == 1u) {
            pitcher_count++;
            if (is_asian) {
                asian_pitcher_count++;
                if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                    asian_starter_pitcher_count++;
                } else if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                    asian_bullpen_pitcher_count++;
                }
            } else {
                non_asian_pitcher_count++;
                if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                    non_asian_starter_pitcher_count++;
                } else if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                    non_asian_bullpen_pitcher_count++;
                }
            }

        }

        quality_policy = kbo_intl_established_fa_quality_policy_label(is_asian, position_group, position_role);
        quality_cap = kbo_intl_established_fa_quality_score_cap(is_asian, position_group, position_role);
        quality_field_cap = kbo_intl_established_fa_quality_field_cap(is_asian, position_group, position_role);
        if (quality_shaping_enabled && quality_cap > 0) {
            int32_t adjusted_score = score;
            was_quality_adjusted = kbo_intl_established_fa_apply_quality_cap(
                player,
                quality_cap,
                quality_field_cap,
                score,
                &adjusted_score);
            if (was_quality_adjusted) {
                score = adjusted_score;
                quality_adjusted_count++;
                if (is_asian) {
                    asian_pitcher_adjusted_count++;
                } else {
                    non_asian_pitcher_adjusted_count++;
                }
                if (position_group == 1u) {
                    if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                        starter_pitcher_adjusted_count++;
                    } else if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                        bullpen_pitcher_adjusted_count++;
                    }
                }
            }
        }

        uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
        uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
        uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
        uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
        int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
        int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
        int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
        int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);

        if (current_team_id == 0u && active_team_id == 0u) {
            teamless++;
        }
        if ((batch->primary_league_id != 0u && current_league_id == batch->primary_league_id)
                || (batch->fallback_league_id != 0u && current_league_id == batch->fallback_league_id)
                || (batch->primary_league_id != 0u && draft_league_id == batch->primary_league_id)
                || (batch->fallback_league_id != 0u && draft_league_id == batch->fallback_league_id)) {
            league_match++;
        }

        score_sum += score;
        if (score > max_score) {
            max_score = score;
        }
        if (is_asian) {
            asian++;
            asian_score_sum += score;
            if (score > asian_max_score) {
                asian_max_score = score;
            }
        } else {
            non_asian++;
            non_asian_score_sum += score;
            if (score > non_asian_max_score) {
                non_asian_max_score = score;
            }
        }

        if (csv_ok) {
            kbo_intl_established_fa_postscan_write_csv_row(
                csv_file,
                date,
                batch->batch_id,
                batch->before_count,
                player_count,
                batch->expected_count,
                i,
                player);
        }

        if (logged < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_DETAIL_LOGS) {
            append_logf(
                "international established FA postscan player batch=%ld index=%d player=%u nation=%u asian_quota=%d age=%d pos=%u/%u team=%u active=%u league=%u draft_league=%u gen=flags:%u context:%u grade:%u special:%u quality=policy:%s cap:%d field_cap:%d original:%d adjusted:%d changed:%d value=overall:%d talent:%d ratings:%d career:%d score=%d",
                batch->batch_id,
                i,
                player_id,
                nation_id,
                is_asian,
                (int)age,
                position_group,
                position_role,
                current_team_id,
                active_team_id,
                current_league_id,
                draft_league_id,
                generation_flags,
                generation_context,
                generation_grade,
                generation_special,
                quality_policy,
                quality_cap,
                (int)quality_field_cap,
                original_score,
                score,
                was_quality_adjusted,
                overall,
                talent,
                ratings,
                career,
                score);
            logged++;
        }
    }

    if (csv_file != INVALID_HANDLE_VALUE) {
        CloseHandle(csv_file);
    }

    int avg = foreign > 0 ? (int)(score_sum / foreign) : 0;
    int asian_avg = asian > 0 ? (int)(asian_score_sum / asian) : 0;
    int non_asian_avg = non_asian > 0 ? (int)(non_asian_score_sum / non_asian) : 0;

    append_logf(
        "international established FA postscan summary batch=%ld date=%s before_count=%d after_count=%d before_max_player=%u original=%d expected=%d matched=%d valid=%d foreign=%d asian=%d non_asian=%d teamless=%d league_match=%d avg_score=%d asian_avg=%d non_asian_avg=%d max_score=%d asian_max=%d non_asian_max=%d quality_shaping=%d pitchers=%d asian_pitchers=%d non_asian_pitchers=%d asian_starters=%d asian_bullpen=%d non_asian_starters=%d non_asian_bullpen=%d adjusted=%d asian_adjusted=%d non_asian_adjusted=%d starter_adjusted=%d bullpen_adjusted=%d csv=%s",
        batch->batch_id,
        date,
        batch->before_count,
        player_count,
        batch->before_max_player_id,
        batch->original_count,
        batch->expected_count,
        matched,
        valid,
        foreign,
        asian,
        non_asian,
        teamless,
        league_match,
        avg,
        asian_avg,
        non_asian_avg,
        max_score,
        asian_max_score,
        non_asian_max_score,
        quality_shaping_enabled ? 1 : 0,
        pitcher_count,
        asian_pitcher_count,
        non_asian_pitcher_count,
        asian_starter_pitcher_count,
        asian_bullpen_pitcher_count,
        non_asian_starter_pitcher_count,
        non_asian_bullpen_pitcher_count,
        quality_adjusted_count,
        asian_pitcher_adjusted_count,
        non_asian_pitcher_adjusted_count,
        starter_pitcher_adjusted_count,
        bullpen_pitcher_adjusted_count,
        csv_ok ? csv_path : "");
}

static void kbo_intl_established_fa_postscan_try_run(void)
{
    if (InterlockedCompareExchange(&g_kbo_intl_established_fa_postscan.pending, 2, 1) != 1) {
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (now < g_kbo_intl_established_fa_postscan.due_tick) {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
        return;
    }

    KboIntlEstablishedFaPostscanState batch = g_kbo_intl_established_fa_postscan;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int vector_ready = find_kbo_global_player_vector(&player_vector, &player_count, NULL);
    if (!vector_ready && batch.attempts < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
        append_logf(
            "international established FA postscan retry batch=%ld attempt=%d reason=no_player_vector",
            batch.batch_id,
            batch.attempts + 1);
        return;
    }

    if (vector_ready
            && batch.before_count > 0
            && batch.expected_count > 0
            && player_count < batch.before_count + batch.expected_count
            && batch.attempts < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
        append_logf(
            "international established FA postscan retry batch=%ld attempt=%d reason=waiting_for_players before=%d after=%d expected=%d",
            batch.batch_id,
            batch.attempts + 1,
            batch.before_count,
            player_count,
            batch.expected_count);
        return;
    }

    kbo_intl_established_fa_postscan_run(&batch);
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 0);
}

static DWORD WINAPI kbo_intl_established_fa_postscan_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("international established FA postscan worker started");
    for (;;) {
        Sleep(1000);
        kbo_intl_established_fa_postscan_try_run();
    }
    return 0;
}

static void start_kbo_intl_established_fa_postscan_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_intl_established_fa_postscan_worker_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_intl_established_fa_postscan_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan_worker_started, 0);
        append_logf("international established FA postscan worker failed to start error=%lu", GetLastError());
    }
}
