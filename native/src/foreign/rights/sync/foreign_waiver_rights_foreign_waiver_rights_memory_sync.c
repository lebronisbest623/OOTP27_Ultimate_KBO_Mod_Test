#include "../internal/foreign_waiver_rights_internal.h"

/* Foreign reserve-right memory synchronization. */

int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd)
{
    if (player == NULL || player_id == 0u || holder_team_id == 0u || today_yyyymmdd == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t league_id = 0u;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id
                && rec->team_id == holder_team_id
                && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            league_id = rec->league_id;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    uint8_t* holder_team = find_kbo_team_by_numeric_id_any_league(holder_team_id, 1);
    if (holder_team == NULL || !memory_range_readable(holder_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    if (league_id == 0u) {
        league_id = *(uint32_t*)(holder_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    if (league_id == 0u) {
        return 0;
    }

    int changed = 0;
    if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != holder_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = holder_team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (kbo_add_player_id_to_team_fixed_array(holder_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
        changed = 1;
    }

    if (changed) {
        static LONG sync_log_count = 0;
        LONG slot = InterlockedIncrement(&sync_log_count);
        if (slot <= 200) {
            append_logf(
                "foreign reserve rights: synced active right to memory player=%u holder_team=%u league=%u today=%u",
                player_id,
                holder_team_id,
                league_id,
                today_yyyymmdd);
        }
    }
    return 1;
}

