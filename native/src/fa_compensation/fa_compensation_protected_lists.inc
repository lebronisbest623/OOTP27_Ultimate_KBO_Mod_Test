static int kbo_persist_fa_compensation_protected_list(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protected_lists_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA protected list persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,signed_on,due_on,generated_on,"
            "protect_count,protected_count,protected_player_ids,protected_player_names,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    int protected_count = candidate_count;
    if (protected_count > (int)rec->protect_count) {
        protected_count = (int)rec->protect_count;
    }
    char prefix[256] = {0};
    int len = snprintf(
        prefix,
        sizeof(prefix),
        "%u,%u,%s,%u,%u,%u,%u,%u,%u,%d,",
        rec->player_id,
        rec->season,
        rec->grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        generated_yyyymmdd,
        rec->protect_count,
        protected_count);
    if (len > 0 && len < (int)sizeof(prefix)) {
        WriteFile(file, prefix, (DWORD)len, &written, NULL);
    }

    char ids[1024] = {0};
    char names[4096] = {0};
    for (int i = 0; i < protected_count; i++) {
        char chunk[32] = {0};
        snprintf(chunk, sizeof(chunk), "%s%u", i == 0 ? "" : ";", candidates[i].player_id);
        strncat(ids, chunk, sizeof(ids) - strlen(ids) - 1u);
        snprintf(chunk, sizeof(chunk), "%s", i == 0 ? "" : ";");
        strncat(names, chunk, sizeof(names) - strlen(names) - 1u);
        strncat(names, candidates[i].player_name, sizeof(names) - strlen(names) - 1u);
    }
    kbo_fa_market_write_csv_text(file, ids);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_market_write_csv_text(file, names);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_protected_list_ai");
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);

    append_logf(
        "KBO FA protected list generated fa_player=%u signing_team=%u original_team=%u grade=%s protected=%d/%u due=%u path=%s",
        rec->player_id,
        rec->signing_team_id,
        rec->original_team_id,
        rec->grade,
        protected_count,
        rec->protect_count,
        due_yyyymmdd,
        path);
    return 1;
}

static int kbo_persist_fa_compensation_protection_debug(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const KboFaProtectedCandidate* auto_protected,
    int auto_protected_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count < 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protection_debug_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA protection debug persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,due_on,generated_on,"
            "list_type,rank,player_id,player_name,score,age,role,reason,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    int protected_count = candidate_count > (int)rec->protect_count ? (int)rec->protect_count : candidate_count;
    for (int i = 0; i < candidate_count; i++) {
        const KboFaProtectedCandidate* candidate = &candidates[i];
        const char* list_type = i < protected_count ? "protected" : "unprotected";
        char prefix[256] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%s,%u,%u,%u,%u,%s,%d,%u,",
            rec->player_id,
            rec->season,
            rec->grade,
            rec->original_team_id,
            rec->signing_team_id,
            due_yyyymmdd,
            generated_yyyymmdd,
            list_type,
            i + 1,
            candidate->player_id);
        if (len > 0 && len < (int)sizeof(prefix)) {
            WriteFile(file, prefix, (DWORD)len, &written, NULL);
        }
        kbo_fa_market_write_csv_text(file, candidate->player_name);
        char suffix[128] = {0};
        len = snprintf(
            suffix,
            sizeof(suffix),
            ",%d,%u,%u,%s,",
            candidate->score,
            (uint32_t)candidate->age,
            (uint32_t)candidate->role,
            candidate->reason);
        if (len > 0 && len < (int)sizeof(suffix)) {
            WriteFile(file, suffix, (DWORD)len, &written, NULL);
        }
        kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_protection_debug");
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    if (auto_protected != NULL && auto_protected_count > 0) {
        for (int i = 0; i < auto_protected_count; i++) {
            const KboFaProtectedCandidate* automatic = &auto_protected[i];
            char prefix[256] = {0};
            int len = snprintf(
                prefix,
                sizeof(prefix),
                "%u,%u,%s,%u,%u,%u,%u,auto_protected,%d,%u,",
                rec->player_id,
                rec->season,
                rec->grade,
                rec->original_team_id,
                rec->signing_team_id,
                due_yyyymmdd,
                generated_yyyymmdd,
                i + 1,
                automatic->player_id);
            if (len > 0 && len < (int)sizeof(prefix)) {
                WriteFile(file, prefix, (DWORD)len, &written, NULL);
            }
            kbo_fa_market_write_csv_text(file, automatic->player_name);
            char suffix[128] = {0};
            len = snprintf(
                suffix,
                sizeof(suffix),
                ",%d,%u,%u,%s,",
                automatic->score,
                (uint32_t)automatic->age,
                (uint32_t)automatic->role,
                automatic->reason);
            if (len > 0 && len < (int)sizeof(suffix)) {
                WriteFile(file, suffix, (DWORD)len, &written, NULL);
            }
            kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_protection_debug");
            WriteFile(file, "\r\n", 2, &written, NULL);
        }
    }

    CloseHandle(file);
    append_logf(
        "KBO FA protection debug written fa_player=%u protected=%d unprotected=%d auto=%d path=%s",
        rec->player_id,
        protected_count,
        candidate_count - protected_count,
        auto_protected_count,
        path);
    return 1;
}
