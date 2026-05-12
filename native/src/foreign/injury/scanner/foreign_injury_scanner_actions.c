#include "foreign_injury_scanner_internal.h"

int kbo_foreign_injury_restore_active_replacement_player(const KboForeignInjuryReplacement* rec, const char* source)
{
    if (rec == NULL
            || rec->status != KBO_FOREIGN_INJURY_STATUS_ACTIVE
            || rec->team_id == 0u
            || rec->replacement_player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
    if (current_team_id == rec->team_id && active_team_id == rec->team_id) {
        return 0;
    }
    if (current_team_id != 0u || active_team_id != 0u || original_team_id != rec->team_id) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = rec->league_id;
    if (league_id == 0u) {
        league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    int added = kbo_add_player_id_to_team_assignment_arrays(team, rec->replacement_player_id);
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->team_id;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = rec->team_id;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == rec->team_id) {
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0u;
    }
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;

    append_logf(
        "foreign injury replacement: restored active replacement source=%s team=%u player=%u added_arrays=%d before_current=%u before_active=%u before_original=%u",
        source != NULL ? source : "",
        rec->team_id,
        rec->replacement_player_id,
        added,
        current_team_id,
        active_team_id,
        original_team_id);
    return 1;
}

static void kbo_foreign_injury_clear_replacement_contract_for_market(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET) = 0u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        int32_t* salaries = (int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
        for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
            salaries[i] = 0;
        }
    }
}

int kbo_foreign_injury_release_replacement_player(uint32_t team_id, uint32_t player_id, const char* source)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = 0u;
    uint32_t original_league_id = 0u;
    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    uint8_t old_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    uint32_t old_contract_status = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET)
        : 0u;

    if (current_team_id == 0u
            && active_team_id == 0u
            && original_team_id != team_id
            && default_team_id != team_id
            && old_contract_level == 0u
            && old_contract_status == 0u) {
        return 0;
    }

    if (current_team_id != 0u
            && current_team_id != team_id
            && active_team_id != team_id
            && original_team_id != team_id) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    int removed = 0;
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        removed = kbo_remove_player_id_from_known_team_roster_arrays(team, player_id);
    }
    if (current_team_id != 0u && current_team_id != team_id) {
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
        if (current_team != NULL && memory_range_readable(current_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            removed += kbo_remove_player_id_from_known_team_roster_arrays(current_team, player_id);
        }
    }

    if (current_team_id == team_id || active_team_id == team_id || original_team_id == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 0u;
    }
    if (active_team_id == team_id || current_team_id == team_id || original_team_id == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0u;
        player[OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET] = 1u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
            && *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && (*(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) == team_id
                || *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) == current_team_id)) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
    }
    kbo_foreign_injury_clear_replacement_contract_for_market(player);
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;

    append_logf(
        "foreign injury replacement: released replacement source=%s team=%u player=%u removed_arrays=%d before_current=%u before_active=%u before_original=%u before_league=%u before_original_league=%u before_default=%u old_contract_level=%u old_contract_status=%u market=1",
        source != NULL ? source : "",
        team_id,
        player_id,
        removed,
        current_team_id,
        active_team_id,
        original_team_id,
        current_league_id,
        original_league_id,
        default_team_id,
        (uint32_t)old_contract_level,
        old_contract_status);
    return 1;
}
