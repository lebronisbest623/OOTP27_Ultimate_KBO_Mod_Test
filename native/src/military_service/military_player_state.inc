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

