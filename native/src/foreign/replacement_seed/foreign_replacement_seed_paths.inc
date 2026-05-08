/* Foreign replacement-player seed path helpers. Included from native/KBOFix.c. */

static int kbo_get_save_foreign_replacement_players_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_replacement_players_seed.csv", out, out_size);
}

static int kbo_get_global_foreign_replacement_players_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_replacement_players_seed.csv", local_app_data);
    return out[0] != '\0';
}

static int kbo_get_save_foreign_replacement_players_resolved_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_replacement_players_resolved.csv", out, out_size);
}

static int kbo_get_current_players_dat_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\players.dat", save_path);
    DWORD attrs = GetFileAttributesA(out);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return 1;
    }

    snprintf(out, out_size, "%s\\data\\players.dat", save_path);
    attrs = GetFileAttributesA(out);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
