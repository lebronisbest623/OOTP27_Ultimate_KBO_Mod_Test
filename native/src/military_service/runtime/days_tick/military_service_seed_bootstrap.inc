static void kbo_military_prewarm_save_scoped_bootstrap_files(const char* save_path)
{
    if (save_path == NULL || save_path[0] == '\0') {
        return;
    }

    kbo_ensure_military_service_seeds_loaded();
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    kbo_ensure_amateur_reputation_seeds_loaded();

    KboFaMarketSeedCase fa_seed_cases[1];
    char fa_seed_path[MAX_PATH] = {0};
    kbo_load_fa_market_seed_cases(fa_seed_cases, 1, fa_seed_path, sizeof(fa_seed_path));

    KboFaRequalificationRecord requalification_records[1];
    kbo_load_fa_requalification_records(requalification_records, 1);

    append_logf(
        "KBO save bootstrap files prewarmed save=%s fa_seed=%s",
        save_path,
        fa_seed_path[0] != '\0' ? fa_seed_path : "-");
}
