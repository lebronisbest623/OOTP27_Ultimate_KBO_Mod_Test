/* All-Star team naming and team-id patching. */

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
