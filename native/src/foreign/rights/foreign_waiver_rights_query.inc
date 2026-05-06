/* Foreign reserve-right lookup helpers. */

static int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd)
{
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }
    static volatile LONG rights_loaded = 0;
    if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
        kbo_load_foreign_waiver_rights();
    }
    int result = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            result = 1;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    return result;
}

static int kbo_find_active_foreign_waiver_holder(uint32_t player_id, uint32_t today_yyyymmdd, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0;
    }
    if (player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    static volatile LONG rights_loaded = 0;
    if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
        kbo_load_foreign_waiver_rights();
    }

    int found = 0;
    uint32_t holder_team_id = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            holder_team_id = rec->team_id;
            found = holder_team_id != 0u;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (found && out_team_id != NULL) {
        *out_team_id = holder_team_id;
    }
    return found;
}
