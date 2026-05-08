static HMODULE kbo_allstar_get_host_exe(const char* label)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("GetModuleHandleA(NULL) failed for %s", label);
        return NULL;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping %s host=%s", label, host);
        return NULL;
    }

    if (!kbo_fix_enabled()) {
        append_logf("%s skipped: opt-in disabled", label);
        return NULL;
    }

    return exe;
}

static int kbo_patch_static_pattern(
    const char* label,
    const uint8_t* pattern,
    size_t pattern_size,
    size_t patch_offset,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t patch_size)
{
    uint8_t* base = find_ootp_executable_pattern(pattern, pattern_size);
    if (base == NULL) {
        return 0;
    }
    return patch_static_bytes(label, base + patch_offset, expected, patch, patch_size);
}
