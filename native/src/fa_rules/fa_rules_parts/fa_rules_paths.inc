static int kbo_fa_rules_get_localappdata_file_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\%s", local_app_data, file_name);
    return out[0] != '\0';
}

static int kbo_fa_rules_resolve_existing_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char path[MAX_PATH] = {0};
    if (kbo_get_save_scoped_data_file("fa_rules.json", path, sizeof(path))
            && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        kbo_fa_rules_copy_text(path, out, out_size);
        return 1;
    }

    if (kbo_fa_rules_get_localappdata_file_path("fa_rules.json", path, sizeof(path))
            && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        kbo_fa_rules_copy_text(path, out, out_size);
        return 1;
    }

    return 0;
}
