/* Military player day counters and status flag helpers. Included from native/src/military_service_loan.inc. */

/* ---- Player military state helpers ---- */

static int32_t kbo_military_days_left(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET, sizeof(int16_t))) {
        return 0;
    }
    return (int32_t)(*(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET));
}

static int32_t kbo_military_effective_days_left(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        uint32_t today_serial = kbo_current_date_serial();
        if (loan->service_return_date_serial != 0u && today_serial != 0u) {
            return kbo_military_days_left_from_return_serial(
                loan->service_return_date_serial,
                today_serial);
        }
    }
    return kbo_military_days_left(player);
}

static uint32_t kbo_military_effective_return_yyyymmdd(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0u;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        if (loan->service_return_date_serial != 0u) {
            return kbo_military_serial_to_yyyymmdd(loan->service_return_date_serial);
        }
    }

    uint32_t today_serial = kbo_current_date_serial();
    int32_t days_left = kbo_military_days_left(player);
    if (today_serial == 0u || days_left <= 0) {
        return 0u;
    }
    return kbo_military_serial_to_yyyymmdd(today_serial + (uint32_t)days_left);
}

static void kbo_set_military_days_left(uint8_t* player, int32_t days_left)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET, sizeof(int16_t))) {
        return;
    }
    *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET) =
        days_left > 0 ? (int16_t)days_left : 0;
}

static void clear_kbo_military_unavailable_flags(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET]           = 0;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0;
    player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET]             = 0;
}

static void clear_kbo_military_status_flags(uint8_t* player)
{
    clear_kbo_military_unavailable_flags(player);
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 0;
    }
}

static void complete_kbo_military_service_status(uint8_t* player)
{
    clear_kbo_military_status_flags(player);
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = 1;
        kbo_set_military_days_left(player, 0);
    }
}

static int kbo_military_service_team_id_matches(uint32_t team_id, uint32_t service_team_id, uint32_t sang_id, uint32_t kpb_id)
{
    return team_id != 0u
        && (team_id == service_team_id
            || (sang_id != 0u && team_id == sang_id)
            || (kpb_id != 0u && team_id == kpb_id));
}

static uint8_t* kbo_military_find_team_from_seed_code(const char* team_code)
{
    if (team_code == NULL || team_code[0] == '\0') {
        return NULL;
    }

    if (team_code[0] >= '0' && team_code[0] <= '9') {
        return find_kbo_team_by_numeric_id_any_league((uint32_t)strtoul(team_code, NULL, 10), 0);
    }
    return find_kbo_team_by_csv_id_any_league(team_code, 0);
}

static int kbo_military_original_team_from_seed(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id)
{
    if (out_original_team_id != NULL) { *out_original_team_id = 0u; }
    if (out_original_league_id != NULL) { *out_original_league_id = 0u; }
    if (player_id == 0u) {
        return 0;
    }

    kbo_ensure_military_service_seeds_loaded();
    kbo_lock_military_service_seeds();
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id != player_id || seed->original_team_code[0] == '\0') {
            continue;
        }

        uint8_t* original_team = kbo_military_find_team_from_seed_code(seed->original_team_code);
        if (original_team == NULL || !memory_range_readable(original_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t original_team_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (kbo_military_service_team_id_matches(original_team_id, service_team_id, sang_id, kpb_id)) {
            continue;
        }

        if (out_original_team_id != NULL) { *out_original_team_id = original_team_id; }
        if (out_original_league_id != NULL) {
            *out_original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        }
        kbo_unlock_military_service_seeds();
        return 1;
    }
    kbo_unlock_military_service_seeds();
    return 0;
}

static int kbo_military_original_team_from_id(
    uint32_t candidate_team_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id)
{
    if (candidate_team_id == 0u
            || kbo_military_service_team_id_matches(candidate_team_id, service_team_id, sang_id, kpb_id)) {
        return 0;
    }

    uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(candidate_team_id, 0);
    if (original_team == NULL || !memory_range_readable(original_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    if (out_original_team_id != NULL) { *out_original_team_id = candidate_team_id; }
    if (out_original_league_id != NULL) {
        *out_original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    return 1;
}

static int kbo_military_resolve_original_team(
    uint8_t* player,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id)
{
    if (out_original_team_id != NULL) { *out_original_team_id = 0u; }
    if (out_original_league_id != NULL) { *out_original_league_id = 0u; }
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t original_team_id = 0u;
    uint32_t original_league_id = 0u;
    if (kbo_military_original_team_from_seed(
            player_id,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id)) {
        if (out_original_team_id != NULL) { *out_original_team_id = original_team_id; }
        if (out_original_league_id != NULL) { *out_original_league_id = original_league_id; }
        return 1;
    }

    int active_index = find_active_kbo_military_loan_index(player_id);
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        if (kbo_military_original_team_from_id(
                loan->original_team_id,
                service_team_id,
                sang_id,
                kpb_id,
                &original_team_id,
                &original_league_id)) {
            if (loan->original_league_id != 0u) {
                original_league_id = loan->original_league_id;
            }
            if (out_original_team_id != NULL) { *out_original_team_id = original_team_id; }
            if (out_original_league_id != NULL) { *out_original_league_id = original_league_id; }
            return 1;
        }
    }

    if (kbo_military_original_team_from_id(
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id)
            || kbo_military_original_team_from_id(
                *(uint32_t*)(player + 0x5c),
                service_team_id,
                sang_id,
                kpb_id,
                &original_team_id,
                &original_league_id)
            || kbo_military_original_team_from_id(
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                service_team_id,
                sang_id,
                kpb_id,
                &original_team_id,
                &original_league_id)) {
        if (out_original_team_id != NULL) { *out_original_team_id = original_team_id; }
        if (out_original_league_id != NULL) { *out_original_league_id = original_league_id; }
        return 1;
    }

    return 0;
}

static void kbo_military_repair_original_team_memory(
    uint8_t* player,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id)
{
    if (player == NULL
            || original_team_id == 0u
            || !kbo_player_pointer_plausible((uintptr_t)player)
            || kbo_military_service_team_id_matches(original_team_id, service_team_id, sang_id, kpb_id)) {
        return;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_slot_team_id = *(uint32_t*)(player + 0x5c);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        if (loan->original_team_id == 0u
                || kbo_military_service_team_id_matches(loan->original_team_id, service_team_id, sang_id, kpb_id)) {
            loan->original_team_id = original_team_id;
        }
        if (original_league_id != 0u && loan->original_league_id == 0u) {
            loan->original_league_id = original_league_id;
        }
    }
    if (active_team_id == 0u
            || kbo_military_service_team_id_matches(active_team_id, service_team_id, sang_id, kpb_id)) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
    }
    if (original_slot_team_id == 0u
            || kbo_military_service_team_id_matches(original_slot_team_id, service_team_id, sang_id, kpb_id)) {
        *(uint32_t*)(player + 0x5c) = original_team_id;
    }
    if (original_league_id != 0u) {
        uint32_t original_league_slot = *(uint32_t*)(player + 0x4c);
        if (original_league_slot == 0u) {
            *(uint32_t*)(player + 0x4c) = original_league_id;
        }
    }
}

