/* Active military-service loan registry. Included from native/src/military_service_loan.inc. */

static int find_active_kbo_military_loan_index(uint32_t player_id)
{
    if (player_id == 0) {
        return -1;
    }

    LONG count = g_active_military_loan_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    for (LONG i = 0; i < count; i++) {
        if (g_active_military_loans[i].player_id == player_id) {
            return (int)i;
        }
    }
    return -1;
}

static void register_active_kbo_military_loan(
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t service_league_id)
{
    if (player_id == 0 || service_team_id == 0) {
        return;
    }

    int existing = find_active_kbo_military_loan_index(player_id);
    if (existing >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[existing];
        loan->player_ptr = player_ptr;
        if (original_team_id != 0 && original_team_id != service_team_id) {
            loan->original_team_id = original_team_id;
        } else if (loan->original_team_id == service_team_id) {
            loan->original_team_id = 0u;
        }
        if (original_league_id != 0) { loan->original_league_id = original_league_id; }
        loan->service_team_id   = service_team_id;
        loan->service_league_id = service_league_id;
        uint32_t today = kbo_current_date_serial();
        int32_t current_days_left = (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr))
            ? kbo_military_days_left((uint8_t*)player_ptr)
            : KBO_MILITARY_SERVICE_DAYS;
        if (loan->service_start_date_serial == 0) {
            if (current_days_left > 0
                    && current_days_left < KBO_MILITARY_SERVICE_DAYS
                    && today > (uint32_t)(KBO_MILITARY_SERVICE_DAYS - current_days_left)) {
                loan->service_start_date_serial = today - (uint32_t)(KBO_MILITARY_SERVICE_DAYS - current_days_left);
            } else {
                loan->service_start_date_serial = today;
            }
        }
        if (loan->service_total_days <= 0) {
            loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        }
        if (loan->service_return_date_serial == 0 && today != 0u) {
            if (current_days_left > 0) {
                loan->service_return_date_serial = today + (uint32_t)current_days_left;
            } else if (loan->service_start_date_serial != 0u) {
                loan->service_return_date_serial = loan->service_start_date_serial + (uint32_t)loan->service_total_days;
            }
        }
        if (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr)
                && current_days_left > KBO_MILITARY_SERVICE_DAYS) {
            kbo_set_military_days_left((uint8_t*)player_ptr, KBO_MILITARY_SERVICE_DAYS);
        }
        return;
    }

    LONG slot = InterlockedIncrement(&g_active_military_loan_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_active_military_loan_count);
        return;
    }

    KboMilitaryActiveLoan* loan = &g_active_military_loans[slot];
    loan->player_id              = player_id;
    loan->player_ptr             = player_ptr;
    loan->original_team_id       = original_team_id != service_team_id ? original_team_id : 0u;
    loan->original_league_id     = original_league_id;
    loan->service_team_id        = service_team_id;
    loan->service_league_id      = service_league_id;
    loan->service_start_date_serial = kbo_current_date_serial();
    loan->service_total_days     = KBO_MILITARY_SERVICE_DAYS;
    int32_t current_days_left = (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr))
        ? kbo_military_days_left((uint8_t*)player_ptr)
        : KBO_MILITARY_SERVICE_DAYS;
    loan->service_return_date_serial = loan->service_start_date_serial != 0u
        ? loan->service_start_date_serial + (uint32_t)(current_days_left > 0 ? current_days_left : KBO_MILITARY_SERVICE_DAYS)
        : 0u;
    if (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr)) {
        kbo_set_military_days_left(
            (uint8_t*)player_ptr,
            current_days_left > 0 ? current_days_left : KBO_MILITARY_SERVICE_DAYS);
    }
}

static void unregister_active_kbo_military_loan(uint32_t player_id)
{
    int index = find_active_kbo_military_loan_index(player_id);
    if (index >= 0) {
        g_active_military_loans[index].player_id = 0;
    }
}

static int kbo_player_is_registered_active_military_loan(uintptr_t player_ptr)
{
    if (player_ptr == 0 || !memory_range_readable(
            (void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    return find_active_kbo_military_loan_index(player_id) >= 0;
}


