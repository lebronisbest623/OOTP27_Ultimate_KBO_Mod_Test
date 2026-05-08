static int kbo_write_foreign_waiver_window(uint32_t start_yyyymmdd, uint32_t end_yyyymmdd, const char* reason)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_event_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign priority negotiation: failed to write window file error=%lu", GetLastError());
        return 0;
    }

    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%08u,%08u\r\n", start_yyyymmdd, end_yyyymmdd);
    DWORD written = 0;
    int ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    if (!ok || !kbo_atomic_commit(file, tmp_path, path)) {
        if (!ok) {
            kbo_atomic_commit(file, tmp_path, path);
            DeleteFileA(path);
        }
        ok = 0;
    }

    static uint32_t last_logged_start = 0u;
    static uint32_t last_logged_end = 0u;
    static int last_logged_ok = -1;
    if (last_logged_start != start_yyyymmdd || last_logged_end != end_yyyymmdd || last_logged_ok != ok) {
        last_logged_start = start_yyyymmdd;
        last_logged_end = end_yyyymmdd;
        last_logged_ok = ok;
        append_logf(
            "foreign priority negotiation: event window persisted start=%u end=%u reason=%s ok=%d path=%s",
            start_yyyymmdd,
            end_yyyymmdd,
            reason == NULL ? "" : reason,
            ok,
            path);
    }
    return ok;
}
