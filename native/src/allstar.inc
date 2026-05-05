typedef void* (__fastcall *OotpStringAssignFn)(void* string_object, const char* text);
typedef void (__fastcall *OotpVectorPushBack)(void* vector, void* value);

typedef struct KboAllstarLayout {
    uint32_t rules_flag_offset;
    uint32_t game_flag_offset;
    uint32_t auto_schedule_offset;
    uint32_t team_a_offset;
    uint32_t team_b_offset;
    uint32_t subleague_array_offset;
    uint32_t subleague_count_offset;
    uint32_t league_id_primary_offset;
    uint32_t league_id_fallback_offset;
} KboAllstarLayout;

static KboAllstarLayout kbo_get_allstar_layout(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (info.ok && info.timestamp == 0x69F75E6Bu && info.size_of_image == 0x03919000u) {
        KboAllstarLayout may_layout = {
            0x242u, 0x45f0u, 0x45f1u, 0x45f8u, 0x45fcu,
            0x4a38u, 0x4a44u, 0x4cc8u, 0x4cc0u
        };
        return may_layout;
    }

    KboAllstarLayout april_layout = {
        0x242u, 0x45e8u, 0x45e9u, 0x45f0u, 0x45f4u,
        0x4a30u, 0x4a3cu, 0x4cc0u, 0x4cc8u
    };
    return april_layout;
}

static uint32_t kbo_allstar_read_u32(uint8_t* base, uint32_t offset)
{
    if (base == NULL || !memory_range_readable(base + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(base + offset);
}

static uintptr_t kbo_find_allstar_league_ptr(uint32_t league_id)
{
    if (league_id == 0u) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };
    KboAllstarLayout layout = kbo_get_allstar_layout();

    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t league_candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, layout.league_id_fallback_offset + 4u)) {
                continue;
            }

            uint8_t* league = (uint8_t*)league_candidate;
            uint32_t legacy_id = 0u;
            if (memory_range_readable(league + OOTP27_KBO_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
                legacy_id = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_ID_OFFSET);
            }
            uint32_t primary_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
            uint32_t fallback_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
            if (legacy_id == league_id || primary_id == league_id || fallback_id == league_id) {
                return league_candidate;
            }
        }
    }

    return 0;
}

static int kbo_allstar_league_context_enabled(uintptr_t league_ptr)
{
    if (!kbo_fix_enabled() || league_ptr == 0) {
        return 0;
    }

    if (is_kbo_historical_league_context(league_ptr)) {
        return 1;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))) {
        return 0;
    }
    if (!memory_range_readable(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    uint32_t year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < 1982u || year > 2200u) {
        return 0;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    uint32_t primary_league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    if (primary_league_id == configured_league_id || fallback_league_id == configured_league_id) {
        return 1;
    }
    if (primary_league_id == OOTP27_KBO_MAIN_LEAGUE_ID || fallback_league_id == OOTP27_KBO_MAIN_LEAGUE_ID) {
        return 1;
    }

    append_logf(
        "KBO all-star context accepted by year fallback league=%p year=%u configured=%u ids=%u/%u source=single_division_allstar",
        (void*)league_ptr,
        year,
        configured_league_id,
        primary_league_id,
        fallback_league_id);
    return 1;
}

static int kbo_allstar_team_matches_league(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (primary_league_id != 0 && team_league_id == primary_league_id) {
        return 1;
    }
    return fallback_league_id != 0 && fallback_league_id != primary_league_id && team_league_id == fallback_league_id;
}

static OotpStringAssignFn get_ootp_string_assign_fn(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    void* assign_fn = (void*)((uintptr_t)exe + OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable(assign_fn, 16)) {
        return NULL;
    }
    return (OotpStringAssignFn)assign_fn;
}

static void kbo_allstar_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }

    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1);
    }

    size_t len = strlen(value);
    while (len > 0) {
        char c = value[len - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        value[--len] = '\0';
    }
}

static void kbo_allstar_csv_read_cell(const char** cursor, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (cursor == NULL || *cursor == NULL) {
        return;
    }

    const char* p = *cursor;
    size_t len = 0;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }

    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (len + 1 < out_size) {
                        out[len++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                if (*p == ',') {
                    p++;
                }
                break;
            }
        } else if (*p == ',') {
            p++;
            break;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }

        if (len + 1 < out_size) {
            out[len++] = *p;
        }
        p++;
    }

    out[len] = '\0';
    kbo_allstar_trim_cell(out);
    *cursor = p;
}

static int kbo_is_allstar_team_header_name(const char* name)
{
    return ascii_equals_ignore_case(name, "allstar_team")
        || ascii_equals_ignore_case(name, "allstarTeam")
        || ascii_equals_ignore_case(name, "all_star_team")
        || ascii_equals_ignore_case(name, "allstar_side")
        || ascii_equals_ignore_case(name, "all_star_side")
        || ascii_equals_ignore_case(name, "allstar_division")
        || ascii_equals_ignore_case(name, "all_star_division")
        || ascii_equals_ignore_case(name, "kbo_allstar_team");
}

static uint8_t kbo_parse_allstar_side(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    if (ascii_equals_ignore_case(text, "1")
            || ascii_equals_ignore_case(text, "east")
            || ascii_equals_ignore_case(text, "eastern")
            || ascii_equals_ignore_case(text, "e")
            || ascii_equals_ignore_case(text, "dong")
            || ascii_equals_ignore_case(text, "donggun")
            || ascii_equals_ignore_case(text, "dong-gun")) {
        return 1;
    }
    if (ascii_equals_ignore_case(text, "2")
            || ascii_equals_ignore_case(text, "west")
            || ascii_equals_ignore_case(text, "western")
            || ascii_equals_ignore_case(text, "w")
            || ascii_equals_ignore_case(text, "seo")
            || ascii_equals_ignore_case(text, "seogun")
            || ascii_equals_ignore_case(text, "seo-gun")) {
        return 2;
    }
    return 0;
}

static void kbo_derive_current_city_from_team_name(const char* name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (name == NULL || name[0] == '\0') {
        return;
    }

    size_t len = 0;
    while (name[len] != '\0' && name[len] != ' ' && name[len] != '\t' && len + 1 < out_size) {
        out[len] = name[len];
        len++;
    }
    out[len] = '\0';
    if ((unsigned char)out[0] == 0xEF
            && (unsigned char)out[1] == 0xBB
            && (unsigned char)out[2] == 0xBF) {
        memmove(out, out + 3, strlen(out + 3) + 1);
    }
    kbo_allstar_trim_cell(out);
}

static void kbo_csv_find_allstar_team_columns(
    const char* header,
    int* year_col,
    int* team_id_col,
    int* name_col,
    int* allstar_col)
{
    const char* cursor = header;
    char cell[256] = {0};
    *year_col = -1;
    *team_id_col = -1;
    *name_col = -1;
    *allstar_col = -1;

    for (int col = 0; col < 160 && *cursor != '\0'; col++) {
        kbo_allstar_csv_read_cell(&cursor, cell, sizeof(cell));
        if (*year_col < 0 && (
                ascii_equals_ignore_case(cell, "yearID")
                || ascii_equals_ignore_case(cell, "year")
                || ascii_equals_ignore_case(cell, "year_id"))) {
            *year_col = col;
        }
        if (*team_id_col < 0 && (
                ascii_equals_ignore_case(cell, "teamID")
                || ascii_equals_ignore_case(cell, "team_id"))) {
            *team_id_col = col;
        }
        if (*name_col < 0 && ascii_equals_ignore_case(cell, "name")) {
            *name_col = col;
        }
        if (*allstar_col < 0 && kbo_is_allstar_team_header_name(cell)) {
            *allstar_col = col;
        }
    }
}

static void kbo_csv_extract_allstar_team_fields(
    const char* line,
    int year_col,
    int team_id_col,
    int name_col,
    int allstar_col,
    uint16_t* year,
    char* team_id,
    size_t team_id_size,
    char* team_name,
    size_t team_name_size,
    char* current_city,
    size_t current_city_size,
    uint8_t* side)
{
    const char* cursor = line;
    char cell[256] = {0};
    char name[128] = {0};
    *year = 0;
    *side = 0;
    if (team_id != NULL && team_id_size > 0) {
        team_id[0] = '\0';
    }
    if (team_name != NULL && team_name_size > 0) {
        team_name[0] = '\0';
    }
    if (current_city != NULL && current_city_size > 0) {
        current_city[0] = '\0';
    }

    int max_col = year_col;
    if (team_id_col > max_col) {
        max_col = team_id_col;
    }
    if (name_col > max_col) {
        max_col = name_col;
    }
    if (allstar_col > max_col) {
        max_col = allstar_col;
    }

    for (int col = 0; col <= max_col && *cursor != '\0'; col++) {
        kbo_allstar_csv_read_cell(&cursor, cell, sizeof(cell));
        if (col == year_col) {
            int parsed_year = atoi(cell);
            if (parsed_year >= 1800 && parsed_year <= 2200) {
                *year = (uint16_t)parsed_year;
            }
        } else if (col == team_id_col && team_id != NULL && team_id_size > 0) {
            snprintf(team_id, team_id_size, "%s", cell);
        } else if (col == name_col) {
            snprintf(name, sizeof(name), "%s", cell);
            if (team_name != NULL && team_name_size > 0) {
                snprintf(team_name, team_name_size, "%s", cell);
            }
        } else if (col == allstar_col) {
            *side = kbo_parse_allstar_side(cell);
        }
    }

    if (current_city != NULL && current_city_size > 0) {
        kbo_derive_current_city_from_team_name(name, current_city, current_city_size);
    }
}

static int get_kbo_allstar_teams_csv_path(char* path, size_t path_size)
{
    if (path == NULL || path_size == 0) {
        return 0;
    }
    path[0] = '\0';

    HMODULE exe = GetModuleHandleA(NULL);
    char host_stats_dir[MAX_PATH] = {0};
    if (exe != NULL) {
        char host[MAX_PATH] = {0};
        DWORD got = GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
        if (got > 0 && got < sizeof(host)) {
            char* slash = strrchr(host, '\\');
            if (slash != NULL) {
                *slash = '\0';
                snprintf(host_stats_dir, sizeof(host_stats_dir), "%s\\data\\stats\\KBO", host);
                snprintf(path, path_size, "%s\\allstar_teams.csv", host_stats_dir);
                if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
                    return 1;
                }
            }
        }
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got > 0 && got < sizeof(local_app_data)) {
        snprintf(path, path_size, "%s\\OOTP-KBO\\allstar_teams.csv", local_app_data);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    if (host_stats_dir[0] != '\0') {
        snprintf(path, path_size, "%s\\Teams.csv", host_stats_dir);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    if (got > 0 && got < sizeof(local_app_data)) {
        snprintf(path, path_size, "%s\\OOTP-KBO\\Teams.csv", local_app_data);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    return path[0] != '\0';
}

static void add_allstar_team_row(uint16_t year, const char* team_id, const char* team_name, const char* current_city, uint8_t side)
{
    if (year < 1800 || year > 2200 || side == 0 || side > 2
            || team_id == NULL || team_id[0] == '\0'
            || g_allstar_team_row_count >= OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS) {
        return;
    }

    for (int i = 0; i < g_allstar_team_row_count; i++) {
        KboAllstarTeamRow* existing = &g_allstar_team_rows[i];
        if (existing->year == year
                && ascii_equals_ignore_case(existing->team_id, team_id)
                && existing->side == side) {
            return;
        }
    }

    KboAllstarTeamRow* row = &g_allstar_team_rows[g_allstar_team_row_count++];
    row->year = year;
    row->side = side;
    snprintf(row->team_id, sizeof(row->team_id), "%s", team_id);
    snprintf(row->team_name, sizeof(row->team_name), "%s", team_name != NULL ? team_name : "");
    snprintf(row->current_city, sizeof(row->current_city), "%s", current_city != NULL ? current_city : "");
}

static void load_allstar_team_rules_once(void)
{
    if (InterlockedCompareExchange(&g_allstar_team_rules_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_allstar_teams_csv_path(path, sizeof(path))) {
        append_log_line("all-star team split load failed: could not resolve allstar_teams.csv path");
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        append_logf("all-star team split load skipped: fopen failed path=%s", path);
        return;
    }

    char line[8192] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        append_logf("all-star team split load failed: empty file path=%s", path);
        return;
    }

    int year_col = -1;
    int team_id_col = -1;
    int name_col = -1;
    int allstar_col = -1;
    kbo_csv_find_allstar_team_columns(line, &year_col, &team_id_col, &name_col, &allstar_col);
    if (year_col < 0 || team_id_col < 0 || allstar_col < 0) {
        fclose(file);
        append_logf(
            "all-star team split load skipped: all-star CSV missing columns year_col=%d team_id_col=%d name_col=%d allstar_col=%d path=%s",
            year_col,
            team_id_col,
            name_col,
            allstar_col,
            path);
        return;
    }

    int rows = 0;
    int loaded = 0;
    int invalid_side = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        uint16_t year = 0;
        char team_id[16] = {0};
        char team_name[96] = {0};
        char current_city[64] = {0};
        uint8_t side = 0;
        kbo_csv_extract_allstar_team_fields(
            line,
            year_col,
            team_id_col,
            name_col,
            allstar_col,
            &year,
            team_id,
            sizeof(team_id),
            team_name,
            sizeof(team_name),
            current_city,
            sizeof(current_city),
            &side);
        rows++;
        if (year == 0 || team_id[0] == '\0') {
            continue;
        }
        if (side == 0) {
            invalid_side++;
            continue;
        }

        int before = g_allstar_team_row_count;
        add_allstar_team_row(year, team_id, team_name, current_city, side);
        if (g_allstar_team_row_count != before) {
            loaded++;
        }
    }

    fclose(file);
    append_logf(
        "all-star team split load: rows=%d loaded=%d invalid_side=%d total=%d year_col=%d team_id_col=%d name_col=%d allstar_col=%d path=%s",
        rows,
        loaded,
        invalid_side,
        g_allstar_team_row_count,
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        path);
}

static void patch_kbo_allstar_team_name(
    uint8_t* team,
    const char* city,
    const char* nickname,
    const char* full_name,
    const char* source)
{
    if (team == NULL || city == NULL || city[0] == '\0'
            || nickname == NULL || nickname[0] == '\0'
            || full_name == NULL || full_name[0] == '\0'
            || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    OotpStringAssignFn assign_string = get_ootp_string_assign_fn();
    if (assign_string == NULL) {
        return;
    }

    char old_name[96] = {0};
    copy_ootp_string_object_text(team, 0x10u, old_name, sizeof(old_name));

    int patched = 0;
    if (memory_range_readable(team + 0x10u, 0x10)) {
        assign_string(team + 0x10u, city);
        patched++;
    }
    if (memory_range_readable(team + 0x28u, 0x10)) {
        assign_string(team + 0x28u, nickname);
        patched++;
    }

    static const uint32_t full_name_offsets[] = {0x40u, 0x58u, 0x70u, 0x100u};
    for (size_t i = 0; i < sizeof(full_name_offsets) / sizeof(full_name_offsets[0]); i++) {
        uint32_t offset = full_name_offsets[i];
        if (memory_range_readable(team + offset, 0x10)) {
            assign_string(team + offset, full_name);
            patched++;
        }
    }

    if (patched > 0) {
        LONG log_index = InterlockedIncrement(&g_allstar_team_name_log_count);
        if (log_index <= 20) {
            append_logf(
                "patched KBO all-star team name source=%s team=%p team_id=%u old=%s new=%s slots=%d",
                source != NULL ? source : "",
                team,
                *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET),
                old_name[0] != '\0' ? old_name : "(blank)",
                full_name,
                patched);
        }
    }
}

static void patch_kbo_allstar_team_pair_names(uint8_t* team_a, uint8_t* team_b, const char* source)
{
    patch_kbo_allstar_team_name(team_a, "Eastern", "All-Stars", "Eastern All-Stars", source);
    patch_kbo_allstar_team_name(team_b, "Western", "All-Stars", "Western All-Stars", source);
}

static uint8_t kbo_allstar_side_for_team(uint8_t* team, uint32_t league_year)
{
    load_allstar_team_rules_once();
    if (team == NULL || g_allstar_team_row_count <= 0) {
        return 0;
    }

    char current_city[64] = {0};
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, current_city, sizeof(current_city));

    uint16_t target_year = 0;
    if (league_year >= 1800 && league_year <= 2200) {
        target_year = (uint16_t)league_year;
    }

    for (int pass = 0; pass < 2; pass++) {
        uint16_t year_to_check = target_year;
        if (pass == 1) {
            year_to_check = 0;
            for (int i = 0; i < g_allstar_team_row_count; i++) {
                uint16_t row_year = g_allstar_team_rows[i].year;
                if (row_year <= target_year && row_year > year_to_check) {
                    year_to_check = row_year;
                }
            }
        }

        if (year_to_check == 0) {
            continue;
        }

        for (int i = 0; i < g_allstar_team_row_count; i++) {
            KboAllstarTeamRow* row = &g_allstar_team_rows[i];
            if (row->year != year_to_check) {
                continue;
            }
            if (team_has_ootp_string_text(team, row->team_id)) {
                return row->side;
            }
            if (row->team_name[0] != '\0' && team_has_ootp_string_text(team, row->team_name)) {
                return row->side;
            }
        }

        for (int i = 0; i < g_allstar_team_row_count; i++) {
            KboAllstarTeamRow* row = &g_allstar_team_rows[i];
            if (row->year != year_to_check || row->current_city[0] == '\0') {
                continue;
            }
            if (current_city[0] != '\0' && ascii_equals_ignore_case(current_city, row->current_city)) {
                return row->side;
            }
        }
    }

    return 0;
}

static int ensure_kbo_allstar_team_ids(uintptr_t league_ptr, const char* source)
{
    if (league_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* team_a_slot = (uint32_t*)(league + layout.team_a_offset);
    uint32_t* team_b_slot = (uint32_t*)(league + layout.team_b_offset);
    uint32_t old_team_a = *team_a_slot;
    uint32_t old_team_b = *team_b_slot;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        append_logf("KBO all-star team scan skipped source=%s league=%p reason=no_global_database", source, league);
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000 || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        append_logf(
            "KBO all-star team scan skipped source=%s league=%p reason=bad_team_vector vector=%p count=%d",
            source,
            league,
            (void*)team_vector,
            team_count);
        return 0;
    }

    uint32_t found[2] = {0, 0};
    uint8_t* found_team[2] = {NULL, NULL};
    int found_count = 0;
    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0 || team[OOTP27_KBO_TEAM_DELETED_OFFSET + 1u] == 0) {
            continue;
        }
        if (!kbo_allstar_team_matches_league(team, league_id, fallback_league_id)) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0) {
            continue;
        }

        if (old_team_a > 0 && old_team_b > 0 && old_team_a != old_team_b && team_id == old_team_a) {
            found[0] = team_id;
            found_team[0] = team;
            continue;
        }
        if (old_team_a > 0 && old_team_b > 0 && old_team_a != old_team_b && team_id == old_team_b) {
            found[1] = team_id;
            found_team[1] = team;
            continue;
        }

        if (old_team_a > 0 && old_team_b > 0 && old_team_a != old_team_b) {
            continue;
        }

        if (found_count < 2 && team_id != found[0]) {
            found[found_count] = team_id;
            found_team[found_count] = team;
            found_count++;
        }
    }

    if (old_team_a > 0 && old_team_b > 0 && old_team_a != old_team_b) {
        if (found_team[0] != NULL && found_team[1] != NULL) {
            patch_kbo_allstar_team_pair_names(found_team[0], found_team[1], source);
            return 1;
        }
        append_logf(
            "KBO all-star existing team ids not found source=%s league=%p league_id=%u/%u old=%u/%u",
            source,
            league,
            league_id,
            fallback_league_id,
            old_team_a,
            old_team_b);
        return 1;
    }

    if (found_count > 0) {
        found_count = 0;
        if (found[0] != 0 && found_team[0] != NULL) {
            found_count++;
        }
        if (found[1] != 0 && found_team[1] != NULL) {
            found_count++;
        }
    }

    if (found_count < 2) {
        append_logf(
            "KBO all-star team scan found too few teams source=%s league=%p league_id=%u/%u old=%u/%u found=%u/%u count=%d",
            source,
            league,
            league_id,
            fallback_league_id,
            old_team_a,
            old_team_b,
            found[0],
            found[1],
            found_count);
        return 0;
    }

    *team_a_slot = found[0];
    *team_b_slot = found[1];
    patch_kbo_allstar_team_pair_names(found_team[0], found_team[1], source);
    append_logf(
        "enabled KBO all-star team ids source=%s league=%p league_id=%u/%u teams %u->%u %u->%u",
        source,
        league,
        league_id,
        fallback_league_id,
        old_team_a,
        found[0],
        old_team_b,
        found[1]);
    return 1;
}

__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr)
{
    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return 0;
    }
    if (left_team_vector == NULL || right_team_vector == NULL || vector_push_back_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.subleague_count_offset + sizeof(uint32_t))) {
        return 0;
    }

    if (*(uint32_t*)(league + layout.subleague_count_offset) != 1) {
        return 0;
    }

    uintptr_t subleague_array = *(uintptr_t*)(league + layout.subleague_array_offset);
    if (subleague_array == 0 || !memory_range_readable((void*)subleague_array, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t subleague = *(uintptr_t*)subleague_array;
    if (subleague == 0 || !memory_range_readable((void*)subleague, 0x50)) {
        return 0;
    }

    uint32_t division_count = *(uint32_t*)(subleague + 0x4c);
    if (division_count != 1) {
        return 0;
    }

    uintptr_t division_array = *(uintptr_t*)(subleague + 0x40);
    if (division_array == 0 || !memory_range_readable((void*)division_array, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t division = *(uintptr_t*)division_array;
    if (division == 0 || !memory_range_readable((void*)division, 0x30)) {
        return 0;
    }

    uintptr_t team_ids_begin = *(uintptr_t*)(division + 0x20);
    uintptr_t team_ids_end = *(uintptr_t*)(division + 0x28);
    if (team_ids_begin == 0 || team_ids_end <= team_ids_begin || ((team_ids_end - team_ids_begin) % sizeof(uint32_t)) != 0) {
        return 0;
    }

    size_t team_id_count = (size_t)((team_ids_end - team_ids_begin) / sizeof(uint32_t));
    if (team_id_count < 2 || team_id_count > 128 || !memory_range_readable((void*)team_ids_begin, (SIZE_T)(team_id_count * sizeof(uint32_t)))) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000 || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    ensure_kbo_allstar_team_ids(league_ptr, "allstar_candidate_team_split");

    uintptr_t eligible_team_ptrs[128] = {0};
    uint32_t eligible_team_ids[128] = {0};
    uint8_t eligible_sides[128] = {0};
    int eligible_count = 0;
    int side_rule_count = 0;
    int unknown_side_count = 0;
    uint32_t left_ids[64] = {0};
    uint32_t right_ids[64] = {0};
    int left_count = 0;
    int right_count = 0;
    OotpVectorPushBack push_back = (OotpVectorPushBack)vector_push_back_ptr;

    for (size_t i = 0; i < team_id_count; i++) {
        uint32_t team_id = *(uint32_t*)(team_ids_begin + (i * sizeof(uint32_t)));
        if (team_id == 0 || team_id > (uint32_t)team_count) {
            continue;
        }

        uintptr_t team_ptr = *(uintptr_t*)(team_vector + (((uintptr_t)team_id - 1u) * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0 || team[OOTP27_KBO_TEAM_DELETED_OFFSET + 1u] != 0) {
            continue;
        }
        if (!kbo_allstar_team_matches_league(team, league_id, fallback_league_id)) {
            continue;
        }

        uint32_t stored_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (stored_team_id != 0 && stored_team_id != team_id) {
            continue;
        }
        if (eligible_count >= (int)(sizeof(eligible_team_ptrs) / sizeof(eligible_team_ptrs[0]))) {
            continue;
        }

        uint8_t side = kbo_allstar_side_for_team(team, *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET));
        eligible_team_ptrs[eligible_count] = team_ptr;
        eligible_team_ids[eligible_count] = team_id;
        eligible_sides[eligible_count] = side;
        eligible_count++;
        if (side == 1 || side == 2) {
            side_rule_count++;
        } else {
            unknown_side_count++;
        }
    }

    for (int i = 0; i < eligible_count; i++) {
        if (eligible_sides[i] == 1) {
            left_count++;
        } else if (eligible_sides[i] == 2) {
            right_count++;
        }
    }

    int used_csv_split = left_count > 0 && right_count > 0 && unknown_side_count == 0;
    left_count = 0;
    right_count = 0;

    for (int i = 0; i < eligible_count; i++) {
        int send_left = 0;
        if (used_csv_split) {
            if (eligible_sides[i] == 0) {
                continue;
            }
            send_left = eligible_sides[i] == 1;
        } else {
            send_left = ((left_count + right_count) & 1) == 0;
        }

        if (send_left) {
            push_back(left_team_vector, (void*)eligible_team_ptrs[i]);
            if (left_count < (int)(sizeof(left_ids) / sizeof(left_ids[0]))) {
                left_ids[left_count] = eligible_team_ids[i];
            }
            left_count++;
        } else {
            push_back(right_team_vector, (void*)eligible_team_ptrs[i]);
            if (right_count < (int)(sizeof(right_ids) / sizeof(right_ids[0]))) {
                right_ids[right_count] = eligible_team_ids[i];
            }
            right_count++;
        }
    }

    if (left_count <= 0 || right_count <= 0) {
        return 0;
    }

    LONG log_index = InterlockedIncrement(&g_allstar_candidate_seed_log_count);
    if (log_index <= 20) {
        append_logf(
            "seeded KBO single-division all-star candidate teams league=%p league_id=%u/%u year=%u division_teams=%zu left_count=%d first_left=%u right_count=%d first_right=%u",
            league,
            league_id,
            fallback_league_id,
            *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET),
            team_id_count,
            left_count,
            left_ids[0],
            right_count,
            right_ids[0]);
        append_logf(
            "KBO all-star candidate team split source=allstar_teams.csv league=%p eligible=%d rule_sides=%d unknown_sides=%d mode=%s",
            league,
            eligible_count,
            side_rule_count,
            unknown_side_count,
            used_csv_split ? "csv" : "fallback_alternating");
    }

    return 1;
}

static void enable_kbo_allstar_flags(uintptr_t league_ptr, const char* source)
{
    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))) {
        return;
    }

    uint8_t old_rules_allstar = league[layout.rules_flag_offset];
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint8_t old_auto_schedule_allstar = league[layout.auto_schedule_offset];

    league[layout.rules_flag_offset] = 1;
    league[layout.game_flag_offset] = 1;
    league[layout.auto_schedule_offset] = 1;
    ensure_kbo_allstar_team_ids(league_ptr, source);

    if (old_rules_allstar != 1 || old_allstar_game != 1 || old_auto_schedule_allstar != 1) {
        uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        append_logf(
            "enabled KBO all-star flags source=%s league=%p year=%u rules 0x%x=%u->1 game 0x%x=%u->1 auto 0x%x=%u->1",
            source,
            league,
            league_year,
            layout.rules_flag_offset,
            old_rules_allstar,
            layout.game_flag_offset,
            old_allstar_game,
            layout.auto_schedule_offset,
            old_auto_schedule_allstar);
    }
}

__declspec(noinline) void ootp_kbo_enable_allstar_setting(uintptr_t league_ptr)
{
    enable_kbo_allstar_flags(league_ptr, "allstar_settings_ui");
}

static void force_kbo_allstar_flags_for_configured_league(const char* source)
{
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
    if (league_ptr == 0) {
        append_logf("KBO all-star force flags skipped source=%s league_id=%u reason=league_not_found", source != NULL ? source : "", league_id);
        return;
    }
    enable_kbo_allstar_flags(league_ptr, source != NULL ? source : "allstar_force_flags");
}

static DWORD WINAPI kbo_allstar_force_retry_thread(LPVOID parameter)
{
    (void)parameter;

    for (int attempt = 1; attempt <= 30; attempt++) {
        uint32_t league_id = kbo_get_foreign_waiver_league_id();
        if (league_id == 0u) {
            league_id = kbo_resolve_kbo_league_id();
        }

        uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
        if (league_ptr != 0) {
            append_logf("KBO all-star force retry found league attempt=%d league_id=%u league=%p", attempt, league_id, (void*)league_ptr);
            enable_kbo_allstar_flags(league_ptr, "startup_retry");
            return 0;
        }

        Sleep(1000);
    }

    append_log_line("KBO all-star force retry gave up: league not found");
    return 0;
}

static void start_kbo_allstar_force_retry_thread(void)
{
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_allstar_force_retry_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("KBO all-star force retry thread started");
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("KBO all-star force retry thread failed to start");
    }
}

__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr)
{
    enable_kbo_allstar_flags(league_ptr, "make_allstar_game_events");
}

__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr)
{
    (void)allstar_team_setup_ptr;

    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (!memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))) {
        return;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint32_t old_team_a = *(uint32_t*)(league + layout.team_a_offset);
    uint32_t old_team_b = *(uint32_t*)(league + layout.team_b_offset);

    enable_kbo_allstar_flags(league_ptr, "allstar_voting_begin");

    LONG log_index = InterlockedIncrement(&g_allstar_voting_prepare_log_count);
    if (log_index <= 20) {
        append_logf(
            "prepared KBO all-star voting begin league=%p league_id=%u/%u year=%u game=%u->%u teams %u/%u->%u/%u",
            league,
            league_id,
            fallback_league_id,
            league_year,
            old_allstar_game,
            league[layout.game_flag_offset],
            old_team_a,
            old_team_b,
            *(uint32_t*)(league + layout.team_a_offset),
            *(uint32_t*)(league + layout.team_b_offset));
    }
}
