/* Military service unresolved seed resolution and load orchestration. Included from native/KBOFix.c. */

static int kbo_resolve_military_service_seeds_locked(void)
{
    int resolved = 0;
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id != 0u || seed->key[0] == '\0') {
            continue;
        }
        uint32_t player_id = 0u;
        if (seed->key[0] >= '0' && seed->key[0] <= '9') {
            player_id = (uint32_t)strtoul(seed->key, NULL, 10);
        } else {
            player_id = kbo_resolve_military_service_seed_key_from_players_dat(seed->key);
        }
        if (player_id != 0u) {
            seed->player_id = player_id;
            resolved++;
        }
    }
    if (resolved > 0) {
        kbo_persist_military_service_resolved_cache_locked();
    }
    return resolved;
}

static void kbo_ensure_military_service_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    char resolved_path[MAX_PATH] = {0};
    char loaded_key[MAX_PATH * 3] = {0};
    kbo_get_save_military_service_seed_path(save_seed_path, sizeof(save_seed_path));
    kbo_get_global_military_service_seed_path(global_seed_path, sizeof(global_seed_path));
    kbo_get_save_military_service_resolved_path(resolved_path, sizeof(resolved_path));
    snprintf(
        loaded_key,
        sizeof(loaded_key),
        "%s|%s|%s",
        save_seed_path,
        global_seed_path,
        resolved_path);

    int should_reload = 0;
    if (InterlockedCompareExchange(&g_kbo_military_service_seed_loaded, 1, 0) == 0) {
        should_reload = 1;
    } else if (_stricmp(g_kbo_military_service_seed_loaded_key, loaded_key) != 0) {
        should_reload = 1;
    }

    if (should_reload) {
        kbo_lock_military_service_seeds();
        g_kbo_military_service_seed_count = 0;
        snprintf(g_kbo_military_service_seed_loaded_key, sizeof(g_kbo_military_service_seed_loaded_key), "%s", loaded_key);
        g_kbo_military_service_seed_last_resolve_tick = GetTickCount64();
        kbo_load_military_service_seed_file_locked(save_seed_path);
        if (global_seed_path[0] != '\0'
                && (save_seed_path[0] == '\0' || _stricmp(save_seed_path, global_seed_path) != 0)) {
            kbo_load_military_service_seed_file_locked(global_seed_path);
        }
        kbo_load_military_service_resolved_cache_locked();
        kbo_resolve_military_service_seeds_locked();
        kbo_unlock_military_service_seeds();
        return;
    }

    ULONGLONG now = GetTickCount64();
    ULONGLONG last_tick = g_kbo_military_service_seed_last_resolve_tick;
    if (last_tick == 0ull || now < last_tick || now - last_tick > 5000ull) {
        g_kbo_military_service_seed_last_resolve_tick = now;
        kbo_lock_military_service_seeds();
        kbo_resolve_military_service_seeds_locked();
        kbo_unlock_military_service_seeds();
    }
}
