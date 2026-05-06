/* Foreign roster audit state capture and comparison helpers. Included from foreign_roster_audit.inc. */

static void kbo_capture_foreign_roster_audit_state(uint8_t* player, KboForeignRosterAuditState* out)
{
    memset(out, 0, sizeof(*out));
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->original_team_id = kbo_get_player_original_team_id(player);
    out->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    out->loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    out->restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    out->secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    out->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    out->loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    out->injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    out->score = kbo_foreign_waiver_value_score(player);
}

static int kbo_foreign_roster_audit_state_changed(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    return old_state->nation_id != new_state->nation_id
        || old_state->current_team_id != new_state->current_team_id
        || old_state->active_team_id != new_state->active_team_id
        || old_state->original_team_id != new_state->original_team_id
        || old_state->current_league_id != new_state->current_league_id
        || old_state->loan_team_id != new_state->loan_team_id
        || old_state->restricted != new_state->restricted
        || old_state->secondary_restricted != new_state->secondary_restricted
        || old_state->dfa != new_state->dfa
        || old_state->loan_active != new_state->loan_active
        || old_state->injury_active != new_state->injury_active
        || old_state->score != new_state->score;
}

static const char* kbo_foreign_roster_audit_change_type(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (old_state == NULL) {
        return "NEW_FOREIGN";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u
            && old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "RELEASE_OBSERVED";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u) {
        return "CURRENT_TEAM_CLEARED";
    }
    if (old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "ACTIVE_TEAM_CLEARED";
    }
    if (old_state->current_team_id == 0u && new_state->current_team_id != 0u) {
        return "CURRENT_TEAM_ASSIGNED";
    }
    if (old_state->restricted != new_state->restricted
            || old_state->secondary_restricted != new_state->secondary_restricted
            || old_state->dfa != new_state->dfa
            || old_state->loan_active != new_state->loan_active
            || old_state->injury_active != new_state->injury_active) {
        return "STATUS_CHANGED";
    }
    if (old_state->current_league_id != new_state->current_league_id
            || old_state->loan_team_id != new_state->loan_team_id) {
        return "ASSIGNMENT_CHANGED";
    }
    return "STATE_CHANGED";
}

static KboForeignRosterAuditState* kbo_find_foreign_roster_audit_state(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
        if (g_kbo_foreign_roster_audit[i].player_id == player_id) {
            return &g_kbo_foreign_roster_audit[i];
        }
    }
    return NULL;
}

