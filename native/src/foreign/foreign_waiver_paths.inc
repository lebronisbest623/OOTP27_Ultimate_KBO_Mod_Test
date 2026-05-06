/* Foreign waiver save-scoped and local path helpers. Included from native/src/foreign_waiver_ai.inc. */

static int get_kbo_foreign_waiver_event_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_negotiation_window.txt", out, out_size);
}

static int kbo_get_foreign_waiver_rights_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_rights.csv", out, out_size);
}

static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_decisions.csv", out, out_size);
}

static int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\asian_quota_nation_ids.txt", local_app_data);
    return 1;
}

static int get_kbo_foreign_waiver_announcement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_announcements.txt", out, out_size);
}
