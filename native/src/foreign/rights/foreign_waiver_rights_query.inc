/* Foreign reserve-right lookup helpers. */

static void kbo_ensure_foreign_waiver_rights_loaded_for_lookup(void)
{
    static volatile LONG rights_loaded = 0;
    static volatile LONG load_in_progress = 0;
    static volatile LONG last_attempt_tick = 0;

    if (InterlockedCompareExchange(&rights_loaded, 0, 0) == 1) {
        return;
    }

    DWORD now = GetTickCount();
    LONG last = InterlockedCompareExchange(&last_attempt_tick, 0, 0);
    if (last != 0 && now - (DWORD)last < 1000u) {
        return;
    }

    if (InterlockedCompareExchange(&load_in_progress, 1, 0) != 0) {
        return;
    }

    InterlockedExchange(&last_attempt_tick, (LONG)now);
    if (kbo_load_foreign_waiver_rights()) {
        InterlockedExchange(&rights_loaded, 1);
    }
    InterlockedExchange(&load_in_progress, 0);
}

static int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd)
{
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
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

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    enum { KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE = 256 };
    typedef struct KboForeignWaiverHolderCacheEntry {
        uint32_t player_id;
        uint32_t today_yyyymmdd;
        uint32_t holder_team_id;
        uint8_t found;
        DWORD tick;
    } KboForeignWaiverHolderCacheEntry;
    static KboForeignWaiverHolderCacheEntry holder_cache[KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE] = {{0}};

    DWORD now = GetTickCount();
    uint32_t slot_index = (player_id ^ (player_id >> 8) ^ today_yyyymmdd) % KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE;
    KboForeignWaiverHolderCacheEntry* cached = &holder_cache[slot_index];
    if (cached->player_id == player_id
            && cached->today_yyyymmdd == today_yyyymmdd
            && cached->tick != 0u
            && now - cached->tick <= 500u) {
        if (cached->found && out_team_id != NULL) {
            *out_team_id = cached->holder_team_id;
        }
        return cached->found ? 1 : 0;
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
    cached->player_id = player_id;
    cached->today_yyyymmdd = today_yyyymmdd;
    cached->holder_team_id = holder_team_id;
    cached->found = found ? 1u : 0u;
    cached->tick = now;
    return found;
}
