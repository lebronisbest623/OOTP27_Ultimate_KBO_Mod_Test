/* Native OOTP loan flag helpers. Included from native/src/military_service_loan.inc. */

/* ---- Native loan helpers ---- */

static int kbo_player_native_on_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) > 0
        && player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0
        && player[OOTP27_PLAYER_DFA_FLAG_OFFSET] == 0
        && player[OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET] != 0;
}

static int kbo_clear_native_player_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    return !kbo_player_native_on_loan(player);
}

