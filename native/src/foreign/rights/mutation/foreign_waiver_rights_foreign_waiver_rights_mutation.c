#include "../internal/foreign_waiver_rights_internal.h"

/* Foreign reserve-right table pruning and upserts. */

void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd)
{
    if (today_yyyymmdd == 0u) {
        return;
    }
    static volatile LONG last_pruned_today = 0;
    LONG today_long = (LONG)today_yyyymmdd;
    if (InterlockedCompareExchange(&last_pruned_today, today_long, today_long) == today_long) {
        return;
    }
    LONG previous = InterlockedExchange(&last_pruned_today, today_long);
    if (previous == today_long) {
        return;
    }
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    int w = 0;
    int removed = 0;
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (kbo_is_foreign_waiver_right_expired(&g_kbo_foreign_waiver_rights[i], today_yyyymmdd)) {
            removed++;
            continue;
        }
        if (w != i) {
            g_kbo_foreign_waiver_rights[w] = g_kbo_foreign_waiver_rights[i];
        }
        w++;
    }
    for (int i = w; i < g_kbo_foreign_waiver_rights_count; i++) {
        memset(&g_kbo_foreign_waiver_rights[i], 0, sizeof(KboForeignWaiverRetention));
    }
    g_kbo_foreign_waiver_rights_count = w;
    if (removed > 0) {
        InterlockedIncrement(&g_kbo_foreign_waiver_rights_generation);
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    if (removed > 0) {
        kbo_log_runtimef("foreign reserve rights: expired=%d today=%u", removed, today_yyyymmdd);
        kbo_persist_foreign_waiver_rights();
    }
}

int kbo_set_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t league_id,
    uint32_t retained_on,
    uint32_t expires_on)
{
    if (team_id == 0u || player_id == 0u || league_id == 0u || retained_on == 0u || expires_on == 0u) {
        return 0;
    }

    int upserted = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
            if (g_kbo_foreign_waiver_rights[i].team_id != team_id) {
                kbo_log_runtimef(
                    "foreign reserve rights: reassigned exclusive holder player=%u old_team=%u new_team=%u",
                    player_id,
                    g_kbo_foreign_waiver_rights[i].team_id,
                    team_id);
            }
            g_kbo_foreign_waiver_rights[i].team_id = team_id;
            g_kbo_foreign_waiver_rights[i].league_id = league_id;
            g_kbo_foreign_waiver_rights[i].retained_on_yyyymmdd = retained_on;
            g_kbo_foreign_waiver_rights[i].expires_on_yyyymmdd = expires_on;
            upserted = 1;
            break;
        }
    }
    if (!upserted && g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
        g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
            player_id,
            team_id,
            league_id,
            retained_on,
            expires_on
        };
        upserted = 1;
    }
    if (upserted) {
        InterlockedIncrement(&g_kbo_foreign_waiver_rights_generation);
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (upserted) {
        kbo_persist_foreign_waiver_rights();
    }
    return upserted;
}

static int kbo_remove_foreign_waiver_right_record(uint32_t team_id, uint32_t player_id)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }

    int removed = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    int w = 0;
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && rec->player_id == player_id) {
            removed++;
            continue;
        }
        if (w != i) {
            g_kbo_foreign_waiver_rights[w] = g_kbo_foreign_waiver_rights[i];
        }
        w++;
    }
    for (int i = w; i < g_kbo_foreign_waiver_rights_count; i++) {
        memset(&g_kbo_foreign_waiver_rights[i], 0, sizeof(KboForeignWaiverRetention));
    }
    g_kbo_foreign_waiver_rights_count = w;
    if (removed > 0) {
        InterlockedIncrement(&g_kbo_foreign_waiver_rights_generation);
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    return removed;
}

static void kbo_clear_foreign_waiver_right_memory_marks(
    uint32_t team_id,
    uint32_t player_id,
    int clear_active_team,
    int clear_dfa)
{
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL) {
        kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id);
    }

    uint32_t current_team_id = 0u;
    uint32_t current_league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(player_id, &current_team_id, &current_league_id);
    (void)current_team_id;
    (void)current_league_id;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;
        if (clear_dfa) {
            player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
        }
        if (clear_active_team
                && *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == team_id) {
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
        }
    }
}

int kbo_clear_foreign_waiver_right(uint32_t team_id, uint32_t player_id)
{
    int removed = kbo_remove_foreign_waiver_right_record(team_id, player_id);
    if (removed > 0) {
        kbo_clear_foreign_waiver_right_memory_marks(team_id, player_id, 1, 0);

        kbo_log_runtimef("foreign reserve rights: released team=%u player=%u removed=%d", team_id, player_id, removed);
        kbo_persist_foreign_waiver_rights();
    }
    return removed;
}

int kbo_consume_foreign_waiver_right_after_holder_signing(uint32_t team_id, uint32_t player_id)
{
    int removed = kbo_remove_foreign_waiver_right_record(team_id, player_id);
    if (removed > 0) {
        kbo_clear_foreign_waiver_right_memory_marks(team_id, player_id, 0, 1);
        kbo_log_runtimef(
            "foreign reserve rights: consumed after holder signing team=%u player=%u removed=%d",
            team_id,
            player_id,
            removed);
        kbo_persist_foreign_waiver_rights();
    }
    return removed;
}

