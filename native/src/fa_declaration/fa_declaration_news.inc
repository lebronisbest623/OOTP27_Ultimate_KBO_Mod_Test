static int kbo_get_fa_declaration_news_marker_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_declaration_news_markers.txt", out, out_size);
}

static int kbo_fa_declaration_news_marker_exists(const char* marker)
{
    if (marker == NULL || marker[0] == '\0') {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_news_marker_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL)) {
        buffer[read] = '\0';
        found = strstr(buffer, marker) != NULL;
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static void kbo_fa_declaration_news_persist_marker(const char* marker, const char* source)
{
    if (marker == NULL || marker[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_news_marker_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "KBO FA declaration news marker skipped source=%s marker=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            marker,
            GetLastError(),
            path);
        return;
    }

    DWORD written = 0;
    WriteFile(file, marker, (DWORD)strlen(marker), &written, NULL);
    WriteFile(file, "\r\n", 2u, &written, NULL);
    CloseHandle(file);
}

static int kbo_fa_declaration_news_candidate_listed(
    const uint32_t* used_ids,
    int used_count,
    uint32_t player_id)
{
    if (used_ids == NULL || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < used_count; i++) {
        if (used_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static const KboFaDeclarationCandidate* kbo_fa_declaration_news_find_best_candidate(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    const uint32_t* used_ids,
    int used_count)
{
    const KboFaDeclarationCandidate* best = NULL;
    for (int i = 0; i < candidate_count; i++) {
        const KboFaDeclarationCandidate* c = &candidates[i];
        if (c->player_id == 0u || (c->declared ? 1 : 0) != declared_filter) {
            continue;
        }
        if (kbo_fa_declaration_news_candidate_listed(used_ids, used_count, c->player_id)) {
            continue;
        }
        if (best == NULL
                || c->score > best->score
                || (c->score == best->score && c->player_id < best->player_id)) {
            best = c;
        }
    }
    return best;
}

static void kbo_fa_declaration_news_append_candidate_line(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidate,
    const char* template_key,
    const char* source)
{
    if (out == NULL || out_size == 0u || candidate == NULL || candidate->player_id == 0u) {
        return;
    }

    const char* name = candidate->player_name[0] != '\0'
        ? candidate->player_name
        : "FA candidate";
    char player_link[160] = {0};
    char team_id_text[16] = {0};
    char score_text[16] = {0};
    snprintf(player_link, sizeof(player_link), "<%s:player#%u>", name, candidate->player_id);
    snprintf(team_id_text, sizeof(team_id_text), "%u", candidate->team_id);
    snprintf(score_text, sizeof(score_text), "%d", candidate->score);

    KboNewsTemplateVar vars[] = {
        { "player_link", player_link },
        { "team_id", team_id_text },
        { "grade", candidate->grade[0] != '\0' ? candidate->grade : "-" },
        { "score", score_text },
        { "reason", candidate->decision_reason[0] != '\0' ? candidate->decision_reason : "-" },
    };

    char rendered[512] = {0};
    if (!kbo_news_template_render_key(
            template_key,
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            source)) {
        snprintf(
            rendered,
            sizeof(rendered),
            "- %s / Team #%u / %s",
            player_link,
            candidate->team_id,
            candidate->grade[0] != '\0' ? candidate->grade : "-");
    }

    kbo_news_text_append(out, out_size, rendered);
    if (!kbo_news_text_ends_with_newline(rendered)) {
        kbo_news_text_append(out, out_size, "\n");
    }
}

static void kbo_fa_declaration_news_build_candidate_list(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    int limit,
    const char* template_key,
    const char* source)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (candidates == NULL || candidate_count <= 0 || limit <= 0) {
        return;
    }

    uint32_t used_ids[16] = {0};
    int listed = 0;
    while (listed < limit && listed < (int)(sizeof(used_ids) / sizeof(used_ids[0]))) {
        const KboFaDeclarationCandidate* candidate = kbo_fa_declaration_news_find_best_candidate(
            candidates,
            candidate_count,
            declared_filter,
            used_ids,
            listed);
        if (candidate == NULL) {
            break;
        }
        used_ids[listed] = candidate->player_id;
        kbo_fa_declaration_news_append_candidate_line(out, out_size, candidate, template_key, source);
        listed++;
    }

    if (listed <= 0) {
        char none[128] = {0};
        if (!kbo_news_template_render_key(
                "fa_declaration.summary.none",
                NULL,
                0,
                none,
                sizeof(none),
                source)) {
            snprintf(none, sizeof(none), "- None");
        }
        kbo_news_text_append(out, out_size, none);
        if (!kbo_news_text_ends_with_newline(none)) {
            kbo_news_text_append(out, out_size, "\n");
        }
    }
}

static int kbo_emit_fa_declaration_summary_news(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared,
    int deferred,
    int deferred_retry,
    int deferred_no_market,
    const char* source)
{
    if (event_yyyymmdd == 0u || season < 1982u || season > 2200u || league_id == 0u
            || candidates == NULL || candidate_count <= 0) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (month == 0u || day == 0u) {
        return 0;
    }

    char marker[96] = {0};
    snprintf(marker, sizeof(marker), "summary|%u|%u", season, league_id);
    if (kbo_fa_declaration_news_marker_exists(marker)) {
        return 0;
    }

    char season_text[16] = {0};
    char declared_text[16] = {0};
    char deferred_text[16] = {0};
    char retry_text[16] = {0};
    char no_market_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", season);
    snprintf(declared_text, sizeof(declared_text), "%d", declared);
    snprintf(deferred_text, sizeof(deferred_text), "%d", deferred);
    snprintf(retry_text, sizeof(retry_text), "%d", deferred_retry);
    snprintf(no_market_text, sizeof(no_market_text), "%d", deferred_no_market);

    char declared_list[4096] = {0};
    char deferred_list[4096] = {0};
    kbo_fa_declaration_news_build_candidate_list(
        declared_list,
        sizeof(declared_list),
        candidates,
        candidate_count,
        1,
        KBO_FA_DECLARATION_NEWS_DECLARED_LIMIT,
        "fa_declaration.summary.declared_line",
        source);
    kbo_fa_declaration_news_build_candidate_list(
        deferred_list,
        sizeof(deferred_list),
        candidates,
        candidate_count,
        0,
        KBO_FA_DECLARATION_NEWS_DEFERRED_LIMIT,
        "fa_declaration.summary.deferred_line",
        source);

    KboNewsTemplateVar vars[] = {
        { "season", season_text },
        { "declared_count", declared_text },
        { "deferred_count", deferred_text },
        { "retry_count", retry_text },
        { "no_market_count", no_market_text },
        { "declared_list", declared_list },
        { "deferred_list", deferred_list },
    };

    char title[180] = {0};
    char body[8192] = {0};
    if (!kbo_news_template_render_key(
                "fa_declaration.summary.title",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                title,
                sizeof(title),
                source)
            || !kbo_news_template_render_key(
                "fa_declaration.summary.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        append_logf(
            "KBO FA declaration news skipped source=%s season=%u league_id=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_fa_declaration_news_persist_marker(marker, source);
    }
    append_logf(
        "KBO FA declaration news source=%s date=%u season=%u league=%u candidates=%d declared=%d deferred=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        season,
        league_id,
        candidate_count,
        declared,
        deferred,
        created);
    return created;
}
