/* String helpers (from league_team_rules) */

static int copy_limited_ascii_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (source == NULL || !memory_range_readable(source, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 512; len++) {
        if (!memory_range_readable(source + len, 1)) {
            break;
        }
        unsigned char c = (unsigned char)source[len];
        if (c == '\0') {
            break;
        }
        if (c < 0x20u || c > 0x7eu) {
            break;
        }
        out[len] = (char)c;
    }

    out[len] = '\0';
    return len > 0;
}

static int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size)
{
    uint8_t* string_object = object_base + string_offset;
    uint8_t* text_slot     = string_object + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET;
    if (!memory_range_readable(text_slot, sizeof(char*))) {
        return 0;
    }

    const char* text = *(const char**)text_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static int team_has_ootp_string_text(uint8_t* team, const char* expected)
{
    if (team == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }

    static const uint32_t string_offsets[] = {
        0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u
    };

    char text[96] = {0};
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        text[0] = '\0';
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && ascii_equals_ignore_case(text, expected)) {
            return 1;
        }
    }

    return 0;
}

/* Team and player lookup (from team_continuity) */

static int kbo_player_pointer_plausible(uintptr_t player_ptr)
{
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player   = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age       = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    return player_id > 0 && player_id < 200000000u && age < 80;
}

static int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset)
{
    if (out_vector != NULL) { *out_vector = 0; }
    if (out_count  != NULL) { *out_count  = 0; }
    if (out_offset != NULL) { *out_offset = 0; }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    int best_matches     = 0;
    uintptr_t best_vector = 0;
    int32_t   best_count  = 0;
    uint32_t  best_offset = 0;

    for (uint32_t offset = 0x20u; offset <= 0x500u; offset += 8u) {
        if (!memory_range_readable((void*)(global + offset), 0x10)) {
            continue;
        }

        uintptr_t vector = *(uintptr_t*)(global + offset);
        int32_t   count  = *(int32_t*)(global + offset + 0x0cu);
        if (vector == 0 || count <= 0 || count > 200000) {
            continue;
        }
        if (!memory_range_readable((void*)vector, (SIZE_T)count * sizeof(uintptr_t))) {
            continue;
        }

        int matches = 0;
        int sample_count = count < 2000 ? count : 2000;
        for (int32_t i = 0; i < sample_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (kbo_player_pointer_plausible(player_ptr)) {
                matches++;
            }
        }

        if (matches > best_matches) {
            best_matches = matches;
            best_vector  = vector;
            best_count   = count;
            best_offset  = offset;
        }
    }

    if (best_matches <= 0 || best_vector == 0 || best_count <= 0) {
        return 0;
    }

    if (out_vector != NULL) { *out_vector = best_vector; }
    if (out_count  != NULL) { *out_count  = best_count;  }
    if (out_offset != NULL) { *out_offset = best_offset; }
    return 1;
}

static uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted)
{
    if (team_id == NULL || team_id[0] == '\0') {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t   team_count  = *(int32_t*)(global  + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (team_has_ootp_string_text(team, team_id)) {
            return team;
        }
    }

    return NULL;
}

static uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    if (team_id == 0) {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t   team_count  = *(int32_t*)(global  + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) == team_id) {
            return team;
        }
    }

    return NULL;
}

/* Roster array helpers */

static int kbo_remove_player_id_from_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    int removed = 0;
    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            ids[i] = 0;
            removed++;
        }
    }
    return removed;
}

static int kbo_add_player_id_to_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            return 1;
        }
    }
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == 0) {
            ids[i] = player_id;
            return 1;
        }
    }
    return 0;
}

static int kbo_remove_player_id_from_known_team_roster_arrays(uint8_t* team, uint32_t player_id)
{
    if (team == NULL || player_id == 0) {
        return 0;
    }

    int removed = 0;
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2DA0_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_33E0_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_3700_OFFSET, player_id);
    return removed;
}

static int kbo_add_player_id_to_team_assignment_arrays(uint8_t* team, uint32_t player_id)
{
    int added = 0;
    added += kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id);
    added += kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    return added;
}

/* Team assignment (from team_continuity) */

typedef uint8_t (__fastcall *OotpTeamAddPlayerFn)(void* team, void* player);
typedef void    (__fastcall *OotpTeamRemovePlayerIdFn)(void* team, uint32_t player_id, uint8_t r2760, uint8_t r2da0, uint8_t r3700);

static OotpTeamAddPlayerFn get_ootp_team_add_player_fn(void)
{
    static int logged = 0;
    if (!logged) {
        logged = 1;
        append_log_line("team add player RVA removed; using fallback array update path.");
    }
    return NULL;
}

static OotpTeamRemovePlayerIdFn get_ootp_team_remove_player_id_fn(void)
{
    static int logged = 0;
    if (!logged) {
        logged = 1;
        append_log_line("team remove player RVA removed; using fallback array update path.");
    }
    return NULL;
}

static void kbo_assign_player_to_team_internal(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int update_organization,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    if (out_called_pre_change != NULL) { *out_called_pre_change = 0; }
    if (out_called_register   != NULL) { *out_called_register   = 0; }
    if (out_called_attach     != NULL) { *out_called_attach     = 0; }

    if (player == NULL || team == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(team, OOTP27_KBO_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return;
    }

    uint32_t to_team_numeric_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (to_team_numeric_id == 0) {
        return;
    }

    uint32_t player_id                  = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id            = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id          = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t original_organization_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0) {
        team_league_id = fallback_league_id;
    }

    uint32_t parent_team_id = 0;
    if (memory_range_readable(team + 0x130, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + 0x130);
    }

    uint8_t* old_team = NULL;
    if (current_team_id != 0 && current_team_id != to_team_numeric_id) {
        old_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
    }

    OotpTeamRemovePlayerIdFn remove_player_id = get_ootp_team_remove_player_id_fn();
    if (old_team != NULL && player_id != 0) {
        if (remove_player_id != NULL) {
            remove_player_id(old_team, player_id, 1, 1, 1);
            if (out_called_pre_change != NULL) { *out_called_pre_change = 1; }
        } else if (kbo_remove_player_id_from_known_team_roster_arrays(old_team, player_id) > 0
                && out_called_pre_change != NULL) {
            *out_called_pre_change = 1;
        }
    }

    uint8_t added_by_ootp = 0;
    OotpTeamAddPlayerFn add_player = get_ootp_team_add_player_fn();
    if (add_player != NULL) {
        added_by_ootp = add_player(team, player);
        if (out_called_register != NULL) { *out_called_register = 1; }
        if (added_by_ootp && out_called_attach != NULL) { *out_called_attach = 1; }
    }

    if (!added_by_ootp) {
        if (player_id != 0) {
            kbo_add_player_id_to_team_assignment_arrays(team, player_id);
        }
        if (current_team_id != 0 && *(uint32_t*)(player + 0x5c) != current_team_id) {
            *(uint32_t*)(player + 0x5c) = current_team_id;
        }
        if (current_league_id != 0 && *(uint32_t*)(player + 0x4c) != current_league_id) {
            *(uint32_t*)(player + 0x4c) = current_league_id;
        }
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)   = to_team_numeric_id;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = team_league_id;
        *(uint16_t*)(player + 0x41) = 0x0100;
    }

    if (update_organization) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) =
            parent_team_id != 0 ? parent_team_id : to_team_numeric_id;
    } else {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_organization_team;
    }
    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = fallback_league_id;
}

static void kbo_assign_player_to_team_like_ootp(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    kbo_assign_player_to_team_internal(
        player, team, fallback_league_id, 1,
        out_called_pre_change, out_called_register, out_called_attach);
}
