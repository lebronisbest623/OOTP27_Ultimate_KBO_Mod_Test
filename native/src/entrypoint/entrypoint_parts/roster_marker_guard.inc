static int kbo_current_save_has_required_roster_marker(const char* source, int log_detail)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        if (log_detail) {
            append_logf("KBO runtime marker guard waiting source=%s reason=current_save_unavailable", source != NULL ? source : "");
        }
        return 0;
    }

    char description_path[MAX_PATH] = {0};
    int path_written = snprintf(description_path, sizeof(description_path), "%s\\description.txt", save_path);
    if (path_written <= 0 || (size_t)path_written >= sizeof(description_path)) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_path_overflow save=%s",
                source != NULL ? source : "",
                save_path);
        }
        return 0;
    }

    HANDLE file = CreateFileA(
        description_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_unavailable gle=%lu path=%s",
                source != NULL ? source : "",
                GetLastError(),
                description_path);
        }
        return 0;
    }

    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 1024 * 1024) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_size_invalid path=%s",
                source != NULL ? source : "",
                description_path);
        }
        CloseHandle(file);
        return 0;
    }

    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size.QuadPart + 1u);
    if (text == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL read_ok = ReadFile(file, text, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!read_ok || read != (DWORD)size.QuadPart) {
        HeapFree(GetProcessHeap(), 0, text);
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_read_failed path=%s",
                source != NULL ? source : "",
                description_path);
        }
        return 0;
    }
    text[read] = '\0';

    int marked = kbo_ascii_contains_ignore_case(text, KBO_REQUIRED_ROSTER_MARKER_URL);
    HeapFree(GetProcessHeap(), 0, text);
    if (marked) {
        append_logf(
            "KBO runtime marker guard passed source=%s save=%s description=%s",
            source != NULL ? source : "",
            save_path,
            description_path);
        return 1;
    }

    if (log_detail) {
        append_logf(
            "KBO runtime marker guard waiting source=%s reason=marker_missing save=%s description=%s",
            source != NULL ? source : "",
            save_path,
            description_path);
    }
    return 0;
}
