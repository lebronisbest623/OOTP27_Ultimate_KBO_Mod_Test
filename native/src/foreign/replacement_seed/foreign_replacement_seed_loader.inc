/* Foreign replacement-player seed lazy-load and resolve orchestration. Included from native/KBOFix.c. */

static int kbo_resolve_foreign_replacement_player_seeds_locked(void)
{
    int resolved = 0;
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->player_id != 0u) {
            continue;
        }
        uint8_t resolved_slot_type = 0u;
        uint32_t player_id = 0u;
        if (seed->key[0] >= '0' && seed->key[0] <= '9') {
            player_id = (uint32_t)strtoul(seed->key, NULL, 10);
        } else {
            player_id = kbo_resolve_foreign_replacement_player_seed_from_players_dat(seed->key, &resolved_slot_type);
        }
        if (player_id != 0u) {
            seed->player_id = player_id;
            if (seed->slot_type == 0u) {
                seed->slot_type = resolved_slot_type;
            }
            resolved++;
        }
    }
    InterlockedExchange64(&g_kbo_foreign_replacement_player_seed_last_resolve_tick, (LONG64)GetTickCount64());
    if (resolved > 0) {
        kbo_persist_foreign_replacement_player_resolved_cache_locked();
    }
    return resolved;
}

static void kbo_ensure_foreign_replacement_player_seeds_loaded(void)
{
    char current_path[MAX_PATH] = {0};
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_seed_path(save_seed_path, sizeof(save_seed_path))) {
        save_seed_path[0] = '\0';
    }
    if (!kbo_get_global_foreign_replacement_players_seed_path(global_seed_path, sizeof(global_seed_path))) {
        global_seed_path[0] = '\0';
    }
    snprintf(current_path, sizeof(current_path), "%s", save_seed_path[0] != '\0' ? save_seed_path : global_seed_path);

    kbo_lock_foreign_replacement_player_seeds();
    if (strcmp(g_kbo_foreign_replacement_player_seed_loaded_path, current_path) != 0) {
        memset(g_kbo_foreign_replacement_player_seeds, 0, sizeof(g_kbo_foreign_replacement_player_seeds));
        g_kbo_foreign_replacement_player_seed_count = 0;
        snprintf(g_kbo_foreign_replacement_player_seed_loaded_path, sizeof(g_kbo_foreign_replacement_player_seed_loaded_path), "%s", current_path);
        if (save_seed_path[0] != '\0') {
            kbo_import_foreign_replacement_player_seed_file_locked(save_seed_path, "save_seed");
        }
        if (global_seed_path[0] != '\0') {
            kbo_import_foreign_replacement_player_seed_file_locked(global_seed_path, "global_seed");
        }
        kbo_load_foreign_replacement_player_resolved_cache_locked();
        kbo_resolve_foreign_replacement_player_seeds_locked();
    } else {
        int unresolved = 0;
        for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
            if (g_kbo_foreign_replacement_player_seeds[i].player_id == 0u) {
                unresolved++;
            }
        }
        LONG64 last_tick = InterlockedCompareExchange64(&g_kbo_foreign_replacement_player_seed_last_resolve_tick, 0, 0);
        ULONGLONG now = GetTickCount64();
        if (unresolved > 0 && (last_tick <= 0 || now < (ULONGLONG)last_tick || now - (ULONGLONG)last_tick > 5000ull)) {
            kbo_resolve_foreign_replacement_player_seeds_locked();
        }
    }
    kbo_unlock_foreign_replacement_player_seeds();
}
