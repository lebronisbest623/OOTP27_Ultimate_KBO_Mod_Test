/* Core save-path and save-scoped data directory helpers. */

static int kbo_path_looks_like_absolute_save_path(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    int absolute = 0;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
            && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        absolute = 1;
    } else if (path[0] == '\\' && path[1] == '\\') {
        absolute = 1;
    }
    if (!absolute) {
        return 0;
    }

    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        len--;
    }
    if (len < 3 || _strnicmp(path + len - 3, ".lg", 3) != 0) {
        return 0;
    }

    char normalized[MAX_PATH] = {0};
    if (len >= sizeof(normalized)) {
        return 0;
    }
    memcpy(normalized, path, len);
    normalized[len] = '\0';

    DWORD attrs = GetFileAttributesA(normalized);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

typedef LONG (WINAPI* KboNtQuerySystemInformationFn)(ULONG, PVOID, ULONG, PULONG);

#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_ARRAY_OFFSET 16u
#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_SIZE 40u
#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_PID_OFFSET 8u
#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_HANDLE_OFFSET 16u

static char g_kbo_cached_current_save_path[MAX_PATH] = {0};
static volatile LONG64 g_kbo_cached_current_save_path_tick = 0;

static void kbo_cache_current_save_path(const char* path)
{
    if (path == NULL || !kbo_path_looks_like_absolute_save_path(path)) {
        return;
    }
    snprintf(g_kbo_cached_current_save_path, sizeof(g_kbo_cached_current_save_path), "%s", path);
    InterlockedExchange64(&g_kbo_cached_current_save_path_tick, (LONG64)GetTickCount64());
}

static int kbo_get_cached_current_save_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || g_kbo_cached_current_save_path[0] == '\0') {
        return 0;
    }
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_cached_current_save_path_tick, 0, 0);
    ULONGLONG now = GetTickCount64();
    if (cached_tick <= 0 || now < (ULONGLONG)cached_tick || now - (ULONGLONG)cached_tick > 5000ull) {
        return 0;
    }
    if (!kbo_path_looks_like_absolute_save_path(g_kbo_cached_current_save_path)) {
        g_kbo_cached_current_save_path[0] = '\0';
        return 0;
    }
    snprintf(out, out_size, "%s", g_kbo_cached_current_save_path);
    return out[0] != '\0';
}

static int kbo_extract_lg_save_path_from_file_path(const char* path, char* out, size_t out_size)
{
    if (path == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    const char* normalized = path;
    if (strncmp(normalized, "\\\\?\\", 4) == 0) {
        normalized += 4;
    }

    const char* marker = NULL;
    for (const char* p = normalized; *p != '\0'; p++) {
        if ((p[0] == '.' || p[0] == '.')
                && (p[1] == 'l' || p[1] == 'L')
                && (p[2] == 'g' || p[2] == 'G')
                && (p[3] == '\0' || p[3] == '\\' || p[3] == '/')) {
            marker = p;
            break;
        }
    }
    if (marker == NULL) {
        return 0;
    }

    size_t len = (size_t)(marker - normalized) + 3u;
    if (len == 0 || len >= out_size) {
        return 0;
    }
    memcpy(out, normalized, len);
    out[len] = '\0';
    return kbo_path_looks_like_absolute_save_path(out);
}

static int kbo_get_current_save_path_from_own_file_handles(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) {
        return 0;
    }
    KboNtQuerySystemInformationFn nt_query =
        (KboNtQuerySystemInformationFn)GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (nt_query == NULL) {
        return 0;
    }

    ULONG buffer_size = 0x10000u;
    BYTE* buffer = NULL;
    LONG status = 0;
    ULONG returned = 0;
    for (int attempt = 0; attempt < 8; attempt++) {
        buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_size);
        if (buffer == NULL) {
            return 0;
        }
        status = nt_query(64u, buffer, buffer_size, &returned);
        if (status == 0) {
            break;
        }
        HeapFree(GetProcessHeap(), 0, buffer);
        buffer = NULL;
        if (status != (LONG)0xC0000004u) {
            return 0;
        }
        buffer_size = returned > buffer_size ? returned + 0x10000u : buffer_size * 2u;
    }
    if (buffer == NULL || status != 0) {
        if (buffer != NULL) {
            HeapFree(GetProcessHeap(), 0, buffer);
        }
        return 0;
    }

    DWORD pid = GetCurrentProcessId();
    ULONGLONG handle_count = *(ULONGLONG*)buffer;
    BYTE* entries = buffer + KBO_SYSTEM_HANDLE_TABLE_ENTRY_ARRAY_OFFSET;
    const SIZE_T entry_size = KBO_SYSTEM_HANDLE_TABLE_ENTRY_SIZE;
    int found = 0;

    for (ULONGLONG i = 0; i < handle_count; i++) {
        BYTE* entry = entries + (SIZE_T)i * entry_size;
        DWORD owner_pid = (DWORD)(*(ULONGLONG*)(entry + KBO_SYSTEM_HANDLE_TABLE_ENTRY_PID_OFFSET));
        if (owner_pid != pid) {
            continue;
        }

        HANDLE handle = (HANDLE)(uintptr_t)(*(ULONGLONG*)(entry + KBO_SYSTEM_HANDLE_TABLE_ENTRY_HANDLE_OFFSET));
        if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
            continue;
        }
        if (GetFileType(handle) != FILE_TYPE_DISK) {
            continue;
        }

        char path[MAX_PATH * 2] = {0};
        DWORD len = GetFinalPathNameByHandleA(handle, path, (DWORD)sizeof(path), FILE_NAME_NORMALIZED);
        if (len == 0 || len >= sizeof(path)) {
            continue;
        }

        char save_path[MAX_PATH] = {0};
        if (kbo_extract_lg_save_path_from_file_path(path, save_path, sizeof(save_path))) {
            snprintf(out, out_size, "%s", save_path);
            append_logf("KBO save path resolved by own file handle path=%s source=%s", out, path);
            found = 1;
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static int kbo_get_current_save_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    if (kbo_get_cached_current_save_path(out, out_size)) {
        return 1;
    }

    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_MESSAGE_SAVE_PATH_OFFSET), 0x20)) {
        copy_ootp_string_object_text((uint8_t*)global, OOTP27_MESSAGE_SAVE_PATH_OFFSET, out, out_size);
    }

    if (!kbo_path_looks_like_absolute_save_path(out)) {
        out[0] = '\0';
    }

    if (out[0] == '\0') {
        char local_app_data[MAX_PATH] = {0};
        DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
        if (got > 0 && got < sizeof(local_app_data)) {
            char cache_path[MAX_PATH] = {0};
            snprintf(
                cache_path,
                sizeof(cache_path),
                "%s\\OOTP-KBO\\current_save_path_%lu.txt",
                local_app_data,
                (unsigned long)GetCurrentProcessId());
            HANDLE file = CreateFileA(
                cache_path,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
            if (file != INVALID_HANDLE_VALUE) {
                DWORD read = 0;
                char cached[MAX_PATH] = {0};
                if (ReadFile(file, cached, (DWORD)sizeof(cached) - 1u, &read, NULL) && read > 0) {
                    cached[read] = '\0';
                    for (DWORD i = 0; i < read; i++) {
                        if (cached[i] == '\r' || cached[i] == '\n') {
                            cached[i] = '\0';
                            break;
                        }
                    }
                    if (kbo_path_looks_like_absolute_save_path(cached)) {
                        snprintf(out, out_size, "%s", cached);
                        kbo_cache_current_save_path(out);
                    }
                }
                CloseHandle(file);
            }
        }
    }

    if (out[0] == '\0') {
        kbo_get_current_save_path_from_own_file_handles(out, out_size);
    }

    if (out[0] != '\0') {
        kbo_cache_current_save_path(out);
    }
    return out[0] != '\0';
}

static void kbo_sanitize_path_component(const char* text, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == NULL || text[0] == '\0') {
        snprintf(out, out_size, "unknown_save");
        return;
    }

    size_t used = 0;
    for (const char* p = text; *p != '\0' && used + 1u < out_size; p++) {
        char ch = *p;
        if ((ch >= 'A' && ch <= 'Z')
                || (ch >= 'a' && ch <= 'z')
                || (ch >= '0' && ch <= '9')
                || ch == '-' || ch == '_' || ch == '.') {
            out[used++] = ch;
        } else {
            out[used++] = '_';
        }
    }
    out[used] = '\0';
    if (out[0] == '\0') {
        snprintf(out, out_size, "unknown_save");
    }
}

static int kbo_get_save_scoped_data_dir(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char root_dir[MAX_PATH] = {0};
    snprintf(root_dir, sizeof(root_dir), "%s\\OOTP-KBO", local_app_data);
    CreateDirectoryA(root_dir, NULL);

    char saves_dir[MAX_PATH] = {0};
    snprintf(saves_dir, sizeof(saves_dir), "%s\\saves", root_dir);
    CreateDirectoryA(saves_dir, NULL);

    char save_path[MAX_PATH] = {0};
    char save_name[96] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    const char* base = strrchr(save_path, '\\');
    const char* slash = strrchr(save_path, '/');
    if (slash != NULL && (base == NULL || slash > base)) {
        base = slash;
    }
    kbo_sanitize_path_component(base != NULL ? base + 1 : save_path, save_name, sizeof(save_name));

    ULONGLONG save_identity = 0ull;
    WIN32_FILE_ATTRIBUTE_DATA save_attrs;
    if (GetFileAttributesExA(save_path, GetFileExInfoStandard, &save_attrs)) {
        FILETIME ft = save_attrs.ftCreationTime;
        if (ft.dwLowDateTime == 0u && ft.dwHighDateTime == 0u) {
            ft = save_attrs.ftLastWriteTime;
        }
        save_identity = ((ULONGLONG)ft.dwHighDateTime << 32) | (ULONGLONG)ft.dwLowDateTime;
    }

    char scoped_save_name[128] = {0};
    if (save_identity != 0ull) {
        snprintf(
            scoped_save_name,
            sizeof(scoped_save_name),
            "%s_%08lx%08lx",
            save_name,
            (unsigned long)(save_identity >> 32),
            (unsigned long)(save_identity & 0xffffffffull));
    } else {
        snprintf(scoped_save_name, sizeof(scoped_save_name), "%s", save_name);
    }

    snprintf(out, out_size, "%s\\%s", saves_dir, scoped_save_name);
    CreateDirectoryA(out, NULL);
    return out[0] != '\0';
}

static int kbo_get_save_scoped_data_file(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char dir[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_dir(dir, sizeof(dir))) {
        return 0;
    }
    snprintf(out, out_size, "%s\\%s", dir, file_name);
    return out[0] != '\0';
}
