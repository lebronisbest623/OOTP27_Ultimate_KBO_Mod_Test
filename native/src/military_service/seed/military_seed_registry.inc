/* Military service seed registry mutation and seed-file loading. Included from native/KBOFix.c. */

static int kbo_add_military_service_seed_locked(const KboMilitaryServiceSeed* seed)
{
    if (seed == NULL || (seed->key[0] == '\0' && seed->player_id == 0u)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* existing = &g_kbo_military_service_seeds[i];
        if ((seed->player_id != 0u && existing->player_id == seed->player_id)
                || (seed->key[0] != '\0' && existing->key[0] != '\0'
                    && _stricmp(existing->key, seed->key) == 0)) {
            if (existing->player_id == 0u) {
                existing->player_id = seed->player_id;
            }
            if (seed->service_team_code[0] != '\0') {
                snprintf(existing->service_team_code, sizeof(existing->service_team_code), "%s", seed->service_team_code);
            }
            if (seed->original_team_code[0] != '\0') {
                snprintf(existing->original_team_code, sizeof(existing->original_team_code), "%s", seed->original_team_code);
            }
            if (seed->service_start_yyyymmdd != 0u) {
                existing->service_start_yyyymmdd = seed->service_start_yyyymmdd;
            }
            if (seed->service_return_yyyymmdd != 0u) {
                existing->service_return_yyyymmdd = seed->service_return_yyyymmdd;
            }
            if (seed->service_total_days > 0) {
                existing->service_total_days = seed->service_total_days;
            }
            return 1;
        }
    }
    if (g_kbo_military_service_seed_count >= KBO_MILITARY_SERVICE_SEED_MAX) {
        return 0;
    }
    g_kbo_military_service_seeds[g_kbo_military_service_seed_count++] = *seed;
    return 1;
}

static int kbo_load_military_service_seed_file_locked(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    int loaded = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        KboMilitaryServiceSeed seed;
        if (kbo_parse_military_service_seed_line(line, &seed)
                && kbo_add_military_service_seed_locked(&seed)) {
            loaded++;
        }
    }
    fclose(fp);
    if (loaded > 0) {
        append_logf("KBO military service seed loaded=%d path=%s", loaded, path);
    }
    return loaded;
}
