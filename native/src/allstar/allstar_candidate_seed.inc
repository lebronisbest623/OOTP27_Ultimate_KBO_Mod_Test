/* All-Star candidate team seeding wrapper. */

__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr)
{
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    kbo_perf_probe_record(
        "allstar_candidate_team_split_enter",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        0);

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

    static volatile LONG perf_success_total = 0;
    static volatile LONG perf_success_last = 0;
    static volatile LONG perf_success_ms = 0;
    static volatile LONG perf_success_max = 0;
    static volatile LONG perf_success_tick = 0;
    kbo_perf_probe_record(
        "allstar_candidate_team_split_success",
        &perf_success_total,
        &perf_success_last,
        &perf_success_ms,
        &perf_success_max,
        &perf_success_tick,
        GetTickCount() - perf_start);

    return 1;
}
