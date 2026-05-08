static int kbo_fa_rules_load(KboFaRules* rules)
{
    if (rules == NULL) {
        return 0;
    }
    kbo_fa_rules_init_defaults(rules);

    char path[MAX_PATH] = {0};
    if (!kbo_fa_rules_resolve_existing_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int loaded = 0;
    if (ReadFile(file, buffer, size, &read, NULL)) {
        buffer[read] = '\0';
        kbo_fa_rules_parse_json(rules, buffer, read);
        kbo_fa_rules_copy_text(path, rules->source_path, sizeof(rules->source_path));
        rules->loaded_from_file = 1u;
        loaded = 1;
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return loaded;
}
