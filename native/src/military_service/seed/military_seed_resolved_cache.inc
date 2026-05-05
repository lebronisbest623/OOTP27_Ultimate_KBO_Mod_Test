/* Military service resolved-cache load and persist logic. Included from native/src/military_service/military_service_seed_store.inc. */

static int kbo_load_military_service_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_military_service_resolved_path(path, sizeof(path))) {
        return 0;
    }
    return kbo_load_military_service_seed_file_locked(path);
}

static int kbo_persist_military_service_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_military_service_resolved_path(path, sizeof(path))) {
        return 0;
    }
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        append_logf("KBO military service resolved cache persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }
    fprintf(fp, "source_key,service_team,original_team,service_return_yyyymmdd,player_id\n");
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id == 0u || seed->key[0] == '\0') {
            continue;
        }
        uint32_t return_yyyymmdd = seed->service_return_yyyymmdd;
        if (return_yyyymmdd == 0u && seed->service_start_yyyymmdd != 0u) {
            return_yyyymmdd = kbo_military_yyyymmdd_add_days(
                seed->service_start_yyyymmdd,
                seed->service_total_days > 0 ? seed->service_total_days : KBO_MILITARY_SERVICE_DAYS);
        }
        fprintf(fp, "%s,%s,%s,%u,%u\n",
            seed->key,
            seed->service_team_code[0] != '\0' ? seed->service_team_code : "SANG",
            seed->original_team_code,
            return_yyyymmdd,
            seed->player_id);
    }
    fclose(fp);
    return 1;
}

