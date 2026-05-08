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
