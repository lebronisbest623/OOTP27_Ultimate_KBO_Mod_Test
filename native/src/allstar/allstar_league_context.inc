/* All-Star league and team context helpers. */

static uint32_t kbo_allstar_read_u32(uint8_t* base, uint32_t offset)
{
    if (base == NULL || !memory_range_readable(base + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(base + offset);
}

static int kbo_allstar_memory_executable(const void* address)
{
    if (address == NULL) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return 0;
    }

    DWORD protect = mbi.Protect & 0xffu;
    return protect == PAGE_EXECUTE
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

static int kbo_allstar_league_vtable_plausible(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !memory_range_readable((void*)league_ptr, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t vtable = *(uintptr_t*)league_ptr;
    if (vtable == 0 || !memory_range_readable((void*)(vtable + 0x30u), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t method = *(uintptr_t*)(vtable + 0x30u);
    return kbo_allstar_memory_executable((void*)method);
}

static int kbo_allstar_league_core_plausible(uintptr_t league_ptr)
{
    if (league_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (!memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))
            || !memory_range_readable((void*)(league_ptr + layout.league_id_fallback_offset), sizeof(uint32_t))
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < 1982u || year > 2200u) {
        return 0;
    }

    uint8_t phase = *(uint8_t*)(league + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (phase > 4u) {
        return 0;
    }

    uint32_t phase_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
    if (phase_year != 0u && (phase_year < 1982u || phase_year > 2200u)) {
        return 0;
    }

    if (!kbo_allstar_league_vtable_plausible(league_ptr)) {
        return 0;
    }

    return 1;
}

static uint8_t kbo_allstar_seed_side_for_team_strings(uint8_t* team, uint32_t league_year)
{
    load_allstar_team_rules_once();
    if (team == NULL || g_allstar_team_row_count <= 0) {
        return 0;
    }

    char current_city[64] = {0};
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, current_city, sizeof(current_city));

    uint16_t target_year = 0;
    if (league_year >= 1800u && league_year <= 2200u) {
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

static int kbo_allstar_team_matches_league_ids(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (primary_league_id != 0u && team_league_id == primary_league_id) {
        return 1;
    }
    return fallback_league_id != 0u && fallback_league_id != primary_league_id && team_league_id == fallback_league_id;
}

static int kbo_allstar_league_has_seeded_division_split(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !kbo_allstar_league_core_plausible(league_ptr)) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.subleague_count_offset + sizeof(uint32_t))) {
        return 0;
    }

    if (*(uint32_t*)(league + layout.subleague_count_offset) != 1u) {
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
    if (division_count != 1u) {
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
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    int side_one = 0;
    int side_two = 0;

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
        if (!kbo_allstar_team_matches_league_ids(team, league_id, fallback_league_id)) {
            continue;
        }

        uint8_t side = kbo_allstar_seed_side_for_team_strings(team, league_year);
        if (side == 1) {
            side_one++;
        } else if (side == 2) {
            side_two++;
        }
    }

    return side_one > 0 && side_two > 0;
}

static uintptr_t kbo_find_allstar_league_ptr(uint32_t league_id)
{
    if (league_id == 0u) {
        return 0;
    }

    uintptr_t imported = (uintptr_t)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
        NULL,
        NULL);
    if (imported != 0 && kbo_allstar_league_core_plausible(imported)) {
        uint8_t* league = (uint8_t*)imported;
        KboAllstarLayout imported_layout = kbo_get_allstar_layout();
        uint32_t primary_id = kbo_allstar_read_u32(league, imported_layout.league_id_primary_offset);
        uint32_t fallback_id = kbo_allstar_read_u32(league, imported_layout.league_id_fallback_offset);
        if (primary_id == league_id || fallback_id == league_id) {
            return imported;
        }
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
            if ((legacy_id == league_id || primary_id == league_id || fallback_id == league_id)
                    && kbo_allstar_league_core_plausible(league_candidate)) {
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
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))
            || !kbo_allstar_league_core_plausible(league_ptr)) {
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
    if (kbo_allstar_league_has_seeded_division_split(league_ptr)) {
        return 1;
    }

    return 0;
}

static int kbo_allstar_team_matches_league(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    return kbo_allstar_team_matches_league_ids(team, primary_league_id, fallback_league_id);
}
