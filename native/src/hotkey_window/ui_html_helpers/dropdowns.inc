static void kbo_webview_append_league_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year)
{
    if (buffer == NULL) {
        return;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t leagues[256] = {0};
    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < 256; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0) { continue; }
        int exists = 0;
        for (int j = 0; j < league_count; j++) {
            if (leagues[j] == league_id) { exists = 1; break; }
        }
        if (!exists) { leagues[league_count++] = league_id; }
    }
    if (league_count <= 0) {
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='dropdown leagueMenu'>");
    for (int i = 0; i < league_count; i++) {
        char label[128] = {0};
        char logo_path[MAX_PATH] = {0};
        kbo_hub_copy_league_display_name_fast(leagues[i], label, sizeof(label));
        kbo_hub_get_league_logo_path(leagues[i], current_year, logo_path, sizeof(logo_path));
        kbo_window_text_appendf(buffer, "<a class='ddItem%s' href='kbo://setleague/%u'>",
            leagues[i] == g_kbo_hub_selected_league_id ? " selected" : "", leagues[i]);
        kbo_webview_append_dropdown_logo(buffer, logo_path);
        kbo_window_text_appendf(buffer, "<span class='ddText'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span></a>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}

static void kbo_webview_append_team_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year)
{
    if (buffer == NULL) {
        return;
    }
    if (g_kbo_hub_selected_league_id == 0) {
        kbo_hub_ensure_valid_selection();
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t teams[512] = {0};
    int filtered_count = 0;
    for (int32_t i = 0; i < team_count && filtered_count < 512; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != g_kbo_hub_selected_league_id) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id != 0) { teams[filtered_count++] = team_id; }
    }
    if (filtered_count <= 0) {
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='dropdown teamMenu'>");
    for (int i = 0; i < filtered_count; i++) {
        char label[128] = {0};
        char logo_path[MAX_PATH] = {0};
        kbo_hub_copy_team_display_name_by_id(teams[i], label, sizeof(label), NULL);
        kbo_hub_get_team_logo_path(teams[i], current_year, logo_path, sizeof(logo_path));
        kbo_window_text_appendf(buffer, "<a class='ddItem%s' href='kbo://setteam/%u'>",
            teams[i] == g_kbo_hub_selected_team_id ? " selected" : "", teams[i]);
        kbo_webview_append_dropdown_logo(buffer, logo_path);
        kbo_window_text_appendf(buffer, "<span class='ddText'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span></a>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}

