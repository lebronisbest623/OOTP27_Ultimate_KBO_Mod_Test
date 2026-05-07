/* Foreign reserve-right decision-team and original-club priority helpers. Included from native/src/foreign_waiver_ai.inc. */

static uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + 0x5cu, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + 0x5cu);
}

static uint32_t kbo_get_foreign_waiver_decision_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    /*
     * OOTP saves can carry an active_team_id for foreign players at league start.
     * That is only the club with priority to decide during the KBO reserve-rights
     * event, not an already exercised reserve right. Stored rights are handled by
     * foreign_waiver_rights.csv; candidate ownership should appear only while the
     * negotiation window is open.
     */
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 0;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (active_team_id != 0u) {
        return active_team_id;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (current_team_id != 0u) {
        return current_team_id;
    }

    return 0u;
}

static int kbo_original_club_priority_window_allows(uint8_t* player, uint32_t team_id, const char* action_name)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 1;
    }

    uint32_t priority_team_id = kbo_get_foreign_waiver_decision_team_id(player);
    if (priority_team_id == 0 || team_id == priority_team_id) {
        return 1;
    }

    uint32_t player_id = 0;
    if (player != NULL && memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    }
    append_logf(
        "foreign priority negotiation: blocked action=%s team=%u player=%u priority_team=%u",
        action_name == NULL ? "" : action_name,
        team_id,
        player_id,
        priority_team_id);
    return 0;
}

