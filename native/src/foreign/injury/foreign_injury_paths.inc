/* Foreign injury replacement file paths. Included from foreign_injury_replacement.inc. */

static int kbo_get_foreign_injury_replacement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_injury_replacements.csv", out, out_size);
}

static int kbo_get_save_foreign_injury_replacement_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_injury_replacements_seed.csv", out, out_size);
}

static int kbo_get_global_foreign_injury_replacement_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\foreign_injury_replacements_seed.csv", local_app_data);
    return out[0] != '\0';
}

