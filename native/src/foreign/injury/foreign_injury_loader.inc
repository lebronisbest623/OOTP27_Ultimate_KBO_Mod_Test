/* Foreign injury replacement lazy-load orchestration. Included from native/KBOFix.c. */

static void kbo_ensure_foreign_injury_replacements_loaded(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return;
    }

    kbo_lock_foreign_injury_replacements();
    if (strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) != 0) {
        kbo_load_foreign_injury_replacements_locked(path);
        uint32_t today = 0u;
        kbo_get_current_yyyymmdd(&today);
        int imported = 0;
        char save_seed_path[MAX_PATH] = {0};
        char global_seed_path[MAX_PATH] = {0};
        if (kbo_get_save_foreign_injury_replacement_seed_path(save_seed_path, sizeof(save_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(save_seed_path, today, "save_seed");
        }
        if (kbo_get_global_foreign_injury_replacement_seed_path(global_seed_path, sizeof(global_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(global_seed_path, today, "global_seed");
        }
        if (imported > 0) {
            kbo_persist_foreign_injury_replacements_locked();
        }
    }
    kbo_unlock_foreign_injury_replacements();
}
