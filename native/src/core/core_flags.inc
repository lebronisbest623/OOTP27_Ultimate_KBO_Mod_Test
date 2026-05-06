/* Core local flag readers. */

static int read_kbo_localappdata_flag_file(const char* file_name)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\%s", local_app_data, file_name);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buffer[16] = {0};
    DWORD read = 0;
    ReadFile(file, buffer, sizeof(buffer) - 1, &read, NULL);
    CloseHandle(file);
    return buffer[0] == '1' || buffer[0] == 'y' || buffer[0] == 'Y'
        || buffer[0] == 't' || buffer[0] == 'T';
}

static int kbo_fix_enabled(void)
{
    /* The DLL is purpose-injected by KBOLauncher, so it is enabled by default.
       Write enable_kbo_fix.txt with "0" to the OOTP-KBO folder to opt out. */
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }

    /* Opt-out check: if the file exists and starts with 0/n/N/f/F, disable. */
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    int enabled = 1;
    if (got > 0 && got < sizeof(local_app_data)) {
        char path[MAX_PATH] = {0};
        snprintf(path, sizeof(path), "%s\\OOTP-KBO\\enable_kbo_fix.txt", local_app_data);
        HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            char buf[16] = {0};
            DWORD rd = 0;
            ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL);
            CloseHandle(f);
            if (buf[0] == '0' || buf[0] == 'n' || buf[0] == 'N' || buf[0] == 'f' || buf[0] == 'F') {
                enabled = 0;
            }
        }
    }

    InterlockedCompareExchange(&cached, enabled ? 1 : 0, -1);
    append_logf("KBO fix opt-in=%d", enabled ? 1 : 0);
    return enabled;
}
