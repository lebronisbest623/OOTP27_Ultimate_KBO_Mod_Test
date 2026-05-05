static RECT kbo_hub_nav_item_rect(int index, int width, int height)
{
    (void)width;
    RECT rect;
    rect.left   = 12 + (g_kbo_hub_skin_article_gap_x / 2);
    rect.right  = rect.left + 156;
    rect.top    = 98 + (index * (34 + g_kbo_hub_skin_article_gap_y));
    rect.bottom = rect.top + 34;
    if (rect.bottom > height - 12) {
        rect.bottom = height - 12;
    }
    return rect;
}

static const char* kbo_hub_current_view_title(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: return kbo_hub_text("\xea\xb5\xb0\xeb\xb3\xb5\xeb\xac\xb4",    "SERVICE TEAMS");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "FOREIGN RIGHTS");
    case KBO_HUB_VIEW_ASIAN_QUOTA: return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\xa0\x95\xec\xb1\x85", "FOREIGN POLICY");
    case KBO_HUB_VIEW_ASIAN_GAMES: return kbo_hub_text("ASIAN GAMES", "ASIAN GAMES");
    case KBO_HUB_VIEW_MOD_INFO: return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",      "SETTINGS");
    default:                    return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    }
}

static const char* kbo_hub_current_view_subtitle(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: return kbo_hub_text("\xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98\xec\x99\x80 \xeb\xb3\xb5\xea\xb7\x80 \xec\x8b\x9c\xec\xa0\x90\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",   "Who is serving and when they return");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xed\x8c\x80\xeb\xb3\x84 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4", "Retained foreign player rights by club");
    case KBO_HUB_VIEW_ASIAN_QUOTA: return kbo_hub_text("\xec\x84\xa0\xed\x83\x9d\xed\x95\x9c \xed\x8c\x80\xec\x9d\x98 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x8a\xac\xeb\xa1\xaf\xea\xb3\xbc \xec\x84\xa0\xec\x88\x98 \xeb\xaa\x85\xeb\x8b\xa8\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4", "Foreign slots and players for the selected club");
    case KBO_HUB_VIEW_ASIAN_GAMES: return kbo_hub_text("ASIAN GAMES ROSTER", "Selected roster, departure, return, and exemption state");
    case KBO_HUB_VIEW_MOD_INFO: return kbo_hub_text("\xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xeb\x9f\xb0\xed\x83\x80\xec\x9e\x84 \xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c\xeb\xa5\xbc \xed\x91\x9c\xec\x8b\x9c\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",       "Launcher and runtime patch status");
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xec\x96\xb8\xec\x96\xb4\xec\x99\x80 \xeb\x8b\xa8\xec\xb6\x95\xed\x82\xa4\xeb\xa5\xbc \xec\x84\xa4\xec\xa0\x95\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",                "Language and hotkey settings");
    default:                    return kbo_hub_text("\xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xeb\x9f\xb0\xed\x83\x80\xec\x9e\x84 \xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c\xeb\xa5\xbc \xed\x91\x9c\xec\x8b\x9c\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",       "Launcher and runtime patch status");
    }
}

static int kbo_hub_league_keeps_allstar_teams(uint32_t league_id)
{
    char league_name[128] = {0};
    kbo_hub_copy_league_display_name(league_id, league_name, sizeof(league_name));
    return kbo_ascii_contains_ignore_case(league_name, "Tournament")
        || kbo_ascii_contains_ignore_case(league_name, "Classic")
        || kbo_ascii_contains_ignore_case(league_name, "Cup")
        || kbo_ascii_contains_ignore_case(league_name, "Qualifier")
        || kbo_ascii_contains_ignore_case(league_name, "World Baseball")
        || kbo_ascii_contains_ignore_case(league_name, "Asian Games");
}

static int kbo_hub_team_name_is_allstar(const char* name)
{
    return kbo_ascii_contains_ignore_case(name, "All-Star")
        || kbo_ascii_contains_ignore_case(name, "All Star")
        || kbo_ascii_contains_ignore_case(name, "Allstars")
        || kbo_ascii_contains_ignore_case(name, "All-Stars")
        || kbo_ascii_contains_ignore_case(name, "All Stars")
        || kbo_ascii_contains_ignore_case(name, "Futures Stars")
        || kbo_ascii_contains_ignore_case(name, "Future Stars");
}

static int kbo_hub_team_hidden_from_dropdown(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (kbo_hub_league_keeps_allstar_teams(league_id)) {
        return 0;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    char display_name[128] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, display_name, sizeof(display_name), NULL);
    if (kbo_hub_team_name_is_allstar(display_name)) {
        return 1;
    }

    static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && kbo_hub_team_name_is_allstar(text)) {
            return 1;
        }
    }

    return 0;
}

static void kbo_hub_ensure_valid_selection(void)
{
    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        g_kbo_hub_selected_league_id = 0;
        g_kbo_hub_selected_team_id   = 0;
        return;
    }

    int has_selected_team   = 0;
    int has_selected_league = 0;
    uint32_t first_league_id = 0;
    uint32_t first_team_id   = 0;
    uint32_t first_team_in_selected_league = 0;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (kbo_hub_team_hidden_from_dropdown(team)) {
            continue;
        }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t team_id   = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (league_id == 0 || team_id == 0) {
            continue;
        }
        if (first_league_id == 0) { first_league_id = league_id; }
        if (first_team_id   == 0) { first_team_id   = team_id;   }
        if (league_id == g_kbo_hub_selected_league_id) {
            has_selected_league = 1;
            if (first_team_in_selected_league == 0) { first_team_in_selected_league = team_id; }
            if (team_id == g_kbo_hub_selected_team_id) { has_selected_team = 1; }
        }
    }

    if (!has_selected_league) {
        g_kbo_hub_selected_league_id           = first_league_id;
        g_kbo_hub_selected_team_id             = 0;
        first_team_in_selected_league          = 0;
        for (int32_t i = 0; i < team_count; i++) {
            uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                continue;
            }
            uint8_t* team = (uint8_t*)team_ptr;
            if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
                continue;
            }
            if (kbo_hub_team_hidden_from_dropdown(team)) {
                continue;
            }
            if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == g_kbo_hub_selected_league_id) {
                first_team_in_selected_league = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
                break;
            }
        }
    }

    if (!has_selected_team) {
        g_kbo_hub_selected_team_id = first_team_in_selected_league != 0
            ? first_team_in_selected_league : first_team_id;
    }
}

static POINT kbo_hub_dropdown_anchor_point(HWND hwnd, const RECT* rect)
{
    POINT pt = {0, 0};
    if (hwnd != NULL
            && rect != NULL
            && rect->right > rect->left
            && rect->bottom > rect->top) {
        pt.x = rect->left;
        pt.y = rect->bottom;
        ClientToScreen(hwnd, &pt);
        return pt;
    }

    if (GetCursorPos(&pt)) {
        return pt;
    }

    RECT window_rect = {0, 0, 0, 0};
    if (hwnd != NULL && GetWindowRect(hwnd, &window_rect)) {
        pt.x = window_rect.left + 24;
        pt.y = window_rect.top + 64;
    }
    return pt;
}

static void kbo_hub_show_league_dropdown(HWND hwnd)
{
    if (hwnd == NULL) {
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

    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    for (int i = 0; i < league_count; i++) {
        char label[128] = {0};
        WCHAR wide_label[160] = {0};
        kbo_hub_copy_league_display_name(leagues[i], label, sizeof(label));
        kbo_utf8_to_wide(label, wide_label, (int)(sizeof(wide_label) / sizeof(wide_label[0])));
        UINT flags = MF_STRING;
        if (leagues[i] == g_kbo_hub_selected_league_id) { flags |= MF_CHECKED; }
        AppendMenuW(menu, flags, 1000u + (UINT)i, wide_label);
    }

    POINT pt = kbo_hub_dropdown_anchor_point(hwnd, &g_kbo_hub_league_dropdown_rect);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= 1000u && command < 1000u + (UINT)league_count) {
        g_kbo_hub_selected_league_id = leagues[command - 1000u];
        g_kbo_hub_selected_team_id   = 0;
        kbo_hub_ensure_valid_selection();
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

static void kbo_hub_show_team_dropdown(HWND hwnd)
{
    if (hwnd == NULL) {
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

    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    for (int i = 0; i < filtered_count; i++) {
        char label[128] = {0};
        WCHAR wide_label[160] = {0};
        kbo_hub_copy_team_display_name_by_id(teams[i], label, sizeof(label), NULL);
        kbo_utf8_to_wide(label, wide_label, (int)(sizeof(wide_label) / sizeof(wide_label[0])));
        UINT flags = MF_STRING;
        if (teams[i] == g_kbo_hub_selected_team_id) { flags |= MF_CHECKED; }
        AppendMenuW(menu, flags, 2000u + (UINT)i, wide_label);
    }

    POINT pt = kbo_hub_dropdown_anchor_point(hwnd, &g_kbo_hub_team_dropdown_rect);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= 2000u && command < 2000u + (UINT)filtered_count) {
        g_kbo_hub_selected_team_id = teams[command - 2000u];
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

static uint32_t kbo_parse_foreign_candidate_player_id_from_line(const char* line)
{
    if (line == NULL || (line[0] != ' ' && line[0] != '>')) {
        return 0u;
    }

    const char* p = line + 1;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0' || strncmp(p, "PLAYER", 6) == 0) {
        return 0u;
    }

    const char* number = line + 27;
    while (*number == ' ') {
        number++;
    }
    if (*number < '0' || *number > '9') {
        return 0u;
    }

    unsigned long raw = strtoul(number, NULL, 10);
    if (raw == 0ul || raw > UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)raw;
}

static int kbo_select_foreign_candidate_from_edit_click(HWND edit, LPARAM lparam)
{
    if (edit == NULL || g_kbo_hub_selected_view != KBO_HUB_VIEW_FOREIGN_RIGHTS) {
        return 0;
    }

    POINT point = {(int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam)};
    LRESULT char_result = SendMessageA(edit, EM_CHARFROMPOS, 0, (LPARAM)&point);
    int char_index = LOWORD(char_result);
    int line_index = (int)SendMessageA(edit, EM_LINEFROMCHAR, (WPARAM)char_index, 0);
    if (line_index < 0) {
        return 0;
    }

    char line[256] = {0};
    *((WORD*)line) = (WORD)(sizeof(line) - 1);
    LRESULT copied = SendMessageA(edit, EM_GETLINE, (WPARAM)line_index, (LPARAM)line);
    if (copied <= 0) {
        return 0;
    }
    if (copied >= (LRESULT)sizeof(line)) {
        copied = (LRESULT)sizeof(line) - 1;
    }
    line[copied] = '\0';

    uint32_t player_id = kbo_parse_foreign_candidate_player_id_from_line(line);
    if (player_id == 0u) {
        return 0;
    }

    g_kbo_hub_selected_foreign_player_id = player_id;
    append_logf("foreign rights ui: selected player=%u from row=%d", player_id, line_index);
    kbo_refresh_hotkey_window();
    if (g_kbo_hotkey_window != NULL) {
        InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
    }
    return 1;
}

static LRESULT CALLBACK kbo_hotkey_edit_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_LBUTTONDOWN && kbo_select_foreign_candidate_from_edit_click(hwnd, lparam)) {
        return 0;
    }

    if (g_kbo_hotkey_edit_original_proc != NULL) {
        return CallWindowProcA(g_kbo_hotkey_edit_original_proc, hwnd, message, wparam, lparam);
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static void kbo_refresh_foreign_rights_controls(void)
{
    if (g_kbo_foreign_list == NULL) {
        return;
    }

    SendMessageA(g_kbo_foreign_list, LB_RESETCONTENT, 0, 0);

    if (g_kbo_hub_selected_view != KBO_HUB_VIEW_FOREIGN_RIGHTS || g_kbo_hub_selected_team_id == 0u) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    int selected_index = -1;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (player_id == 0u
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || (current_team_id != g_kbo_hub_selected_team_id
                    && active_team_id != g_kbo_hub_selected_team_id
                    && !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id))) {
            continue;
        }

        char player_name[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

        char label[192] = {0};
        snprintf(
            label,
            sizeof(label),
            "%-28.28s   ID %-8u   Nation %-4u   %s%s%s",
            player_name,
            player_id,
            nation_id,
            restricted ? "Restricted " : "",
            dfa ? "DFA " : "",
            inj_active ? "Injured" : "Active");

        LRESULT index = SendMessageA(g_kbo_foreign_list, LB_ADDSTRING, 0, (LPARAM)label);
        if (index >= 0) {
            SendMessageA(g_kbo_foreign_list, LB_SETITEMDATA, (WPARAM)index, (LPARAM)player_id);
            if (player_id == g_kbo_hub_selected_foreign_player_id) {
                selected_index = (int)index;
            }
        }
    }

    if (selected_index >= 0) {
        SendMessageA(g_kbo_foreign_list, LB_SETCURSEL, (WPARAM)selected_index, 0);
    } else if (SendMessageA(g_kbo_foreign_list, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageA(g_kbo_foreign_list, LB_SETCURSEL, 0, 0);
        g_kbo_hub_selected_foreign_player_id = (uint32_t)SendMessageA(g_kbo_foreign_list, LB_GETITEMDATA, 0, 0);
    }
}

static void kbo_apply_foreign_rights_button(int retain)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        append_logf("foreign rights ui: %s blocked by window closed", retain ? "RETAIN" : "SKIP");
        return;
    }

    uint32_t player_id = g_kbo_hub_selected_foreign_player_id;
    if (g_kbo_foreign_list != NULL) {
        LRESULT sel = SendMessageA(g_kbo_foreign_list, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR) {
            LRESULT data = SendMessageA(g_kbo_foreign_list, LB_GETITEMDATA, (WPARAM)sel, 0);
            if (data != LB_ERR && data != 0) {
                player_id = (uint32_t)data;
                g_kbo_hub_selected_foreign_player_id = player_id;
            }
        }
    }

    if (player_id != 0u && kbo_append_foreign_waiver_user_decision(g_kbo_hub_selected_team_id, player_id, retain)) {
        append_logf("foreign rights ui: queued %s team=%u player=%u", retain ? "RETAIN" : "SKIP", g_kbo_hub_selected_team_id, player_id);
        kbo_refresh_foreign_rights_controls();
        kbo_refresh_hotkey_window();
        if (g_kbo_hotkey_window != NULL) {
            InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
        }
    }
}


static void kbo_html_append_escaped(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL || text == NULL) {
        return;
    }
    for (const char* p = text; *p != '\0'; p++) {
        switch (*p) {
        case '&': kbo_window_text_appendf(buffer, "&amp;"); break;
        case '<': kbo_window_text_appendf(buffer, "&lt;"); break;
        case '>': kbo_window_text_appendf(buffer, "&gt;"); break;
        case '"': kbo_window_text_appendf(buffer, "&quot;"); break;
        case '\'': kbo_window_text_appendf(buffer, "&#39;"); break;
        default: kbo_window_text_appendf(buffer, "%c", *p); break;
        }
    }
}

static void kbo_webview_append_player_name_cell(KboWindowTextBuffer* buffer, const char* player_name, uint32_t player_id)
{
    kbo_window_text_appendf(buffer, "<td class='pname'>");
    if (player_id != 0u) {
        kbo_window_text_appendf(buffer, "<a title='Open player profile' href='kbo://player/%u'>", player_id);
    }
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player");
    if (player_id != 0u) {
        kbo_window_text_appendf(buffer, "</a>");
    }
    kbo_window_text_appendf(buffer, "</td>");
}

static void kbo_webview_append_team_cell(KboWindowTextBuffer* buffer, const char* team_text)
{
    kbo_window_text_appendf(buffer, "<td class='team'>");
    kbo_html_append_escaped(buffer, team_text != NULL && team_text[0] != '\0' ? team_text : "-");
    kbo_window_text_appendf(buffer, "</td>");
}

static int kbo_hub_days_until_yyyymmdd(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return -1;
    }
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return -1;
    }
    uint32_t today_serial = kbo_date_serial(year, month, day);
    uint32_t end_serial = kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
    if (today_serial == 0u || end_serial == 0u) {
        return -1;
    }
    return end_serial > today_serial ? (int)(end_serial - today_serial) : 0;
}

static const char* kbo_hub_weekday_abbrev(int weekday)
{
    static const char* names[] = { "SUN.", "MON.", "TUE.", "WED.", "THU.", "FRI.", "SAT." };
    if (weekday < 0 || weekday > 6) {
        return "";
    }
    return names[weekday];
}

static const char* kbo_hub_month_abbrev(uint32_t month)
{
    static const char* names[] = {
        "", "JAN.", "FEB.", "MAR.", "APR.", "MAY", "JUN.",
        "JUL.", "AUG.", "SEP.", "OCT.", "NOV.", "DEC."
    };
    if (month < 1u || month > 12u) {
        return "";
    }
    return names[month];
}

static const char* kbo_hub_day_suffix(uint32_t day)
{
    if ((day % 100u) >= 11u && (day % 100u) <= 13u) {
        return "TH";
    }
    switch (day % 10u) {
    case 1u: return "ST";
    case 2u: return "ND";
    case 3u: return "RD";
    default: return "TH";
    }
}

static void kbo_hub_format_ootp_date(uint32_t year, uint32_t month, uint32_t day, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (year < 1900u || month < 1u || month > 12u || day < 1u || day > 31u) {
        snprintf(out, out_size, "DATE UNKNOWN");
        return;
    }

    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        snprintf(out, out_size, "DATE UNKNOWN");
        return;
    }

    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    int weekday = (int)((days_since_1601 + 1ull) % 7ull);
    snprintf(out, out_size, "%s %s %u%s, %u",
        kbo_hub_weekday_abbrev(weekday),
        kbo_hub_month_abbrev(month),
        day,
        kbo_hub_day_suffix(day),
        year);
}

static const char* kbo_hub_flag_entity_for_nation_id(uint32_t nation_id, const char** out_label)
{
    const char* label = "Unknown nation";
    const char* flag = "&#x1F3F3;";
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: label = "Korea";             flag = "&#x1F1F0;&#x1F1F7;"; break;
    case 12u:                        label = "Australia";         flag = "&#x1F1E6;&#x1F1FA;"; break;
    case 36u:                        label = "Canada";            flag = "&#x1F1E8;&#x1F1E6;"; break;
    case 43u:                        label = "Taiwan";            flag = "&#x1F1F9;&#x1F1FC;"; break;
    case 49u:                        label = "Cuba";              flag = "&#x1F1E8;&#x1F1FA;"; break;
    case 56u:                        label = "Dominican Republic"; flag = "&#x1F1E9;&#x1F1F4;"; break;
    case 98u:                        label = "Japan";             flag = "&#x1F1EF;&#x1F1F5;"; break;
    case 124u:                       label = "Mexico";            flag = "&#x1F1F2;&#x1F1FD;"; break;
    case 206u:                       label = "United States";     flag = "&#x1F1FA;&#x1F1F8;"; break;
    case 210u:                       label = "Venezuela";         flag = "&#x1F1FB;&#x1F1EA;"; break;
    default: break;
    }
    if (out_label != NULL) {
        *out_label = label;
    }
    return flag;
}

static void kbo_webview_append_nation_flag_cell(KboWindowTextBuffer* buffer, uint32_t nation_id)
{
    const char* label = NULL;
    const char* flag = kbo_hub_flag_entity_for_nation_id(nation_id, &label);
    kbo_window_text_appendf(buffer, "<td class='flag' title='%s nation#%u'>%s</td>",
        label != NULL ? label : "Unknown nation",
        nation_id,
        flag);
}

static void kbo_webview_append_candidate_card(
    KboWindowTextBuffer* buffer,
    uint8_t* player,
    uint32_t player_id,
    uint32_t current_team_id,
    uint32_t nation_id,
    const char* flags)
{
    char player_name[96] = {0};
    char team_abbrev[16] = {0};
    kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
    kbo_hub_copy_team_abbrev_by_id(current_team_id, team_abbrev, sizeof(team_abbrev), NULL);
    kbo_window_text_appendf(buffer,
        "<tr><td class='toggle'><a class='switch keep' title='Retain rights' href='kbo://retain/%u'>&#9679;</a><a class='switch release' title='Do not retain rights' href='kbo://release/%u'>&#9675;</a></td>",
        player_id,
        player_id);
    kbo_webview_append_player_name_cell(buffer, player_name, player_id);
    kbo_webview_append_team_cell(buffer, team_abbrev);
    kbo_webview_append_nation_flag_cell(buffer, nation_id);
    kbo_window_text_appendf(buffer, "<td>");
    kbo_html_append_escaped(buffer, flags != NULL && flags[0] != '\0' ? flags : "Active");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_webview_append_file_url(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    kbo_window_text_appendf(buffer, "file:///");
    for (const char* p = path; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\\') {
            kbo_window_text_appendf(buffer, "/");
        } else if (ch == ' ') {
            kbo_window_text_appendf(buffer, "%%20");
        } else if (ch == '#') {
            kbo_window_text_appendf(buffer, "%%23");
        } else if (ch == '%') {
            kbo_window_text_appendf(buffer, "%%25");
        } else {
            kbo_window_text_appendf(buffer, "%c", ch);
        }
    }
}

static const char* kbo_webview_image_mime_for_path(const char* path)
{
    if (path == NULL) {
        return NULL;
    }
    size_t len = strlen(path);
    if (len >= 4 && ascii_equals_ignore_case(path + len - 4, ".png")) {
        return "image/png";
    }
    if (len >= 3 && ascii_equals_ignore_case(path + len - 3, ".oi")) {
        return "image/png";
    }
    if (len >= 4 && ascii_equals_ignore_case(path + len - 4, ".jpg")) {
        return "image/jpeg";
    }
    if (len >= 5 && ascii_equals_ignore_case(path + len - 5, ".jpeg")) {
        return "image/jpeg";
    }
    return NULL;
}

static void kbo_webview_append_image_src(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    const char* mime = kbo_webview_image_mime_for_path(path);
    if (mime == NULL) {
        kbo_webview_append_file_url(buffer, path);
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > 1024 * 1024) {
        fclose(file);
        return;
    }
    rewind(file);

    unsigned char* data = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)file_size);
    if (data == NULL) {
        fclose(file);
        return;
    }
    size_t read = fread(data, 1, (size_t)file_size, file);
    fclose(file);
    if (read != (size_t)file_size) {
        HeapFree(GetProcessHeap(), 0, data);
        return;
    }

    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    kbo_window_text_appendf(buffer, "data:%s;base64,", mime);
    for (size_t i = 0; i < read; i += 3) {
        unsigned int value = ((unsigned int)data[i]) << 16;
        int remaining = (int)(read - i);
        if (remaining > 1) { value |= ((unsigned int)data[i + 1]) << 8; }
        if (remaining > 2) { value |= ((unsigned int)data[i + 2]); }
        kbo_window_text_appendf(buffer, "%c%c%c%c",
            alphabet[(value >> 18) & 0x3f],
            alphabet[(value >> 12) & 0x3f],
            remaining > 1 ? alphabet[(value >> 6) & 0x3f] : '=',
            remaining > 2 ? alphabet[value & 0x3f] : '=');
    }
    HeapFree(GetProcessHeap(), 0, data);
}

static void kbo_webview_append_dropdown_logo(KboWindowTextBuffer* buffer, const char* path)
{
    kbo_window_text_appendf(buffer, "<span class='ddLogo'>");
    if (path != NULL && path[0] != '\0') {
        kbo_window_text_appendf(buffer, "<img src='");
        kbo_webview_append_image_src(buffer, path);
        kbo_window_text_appendf(buffer, "'>");
    }
    kbo_window_text_appendf(buffer, "</span>");
}

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
        kbo_hub_copy_league_display_name(leagues[i], label, sizeof(label));
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

static WCHAR* kbo_build_webview_hub_html(void)
{
    const size_t html_cap = 1572864;
    char* html = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, html_cap);
    if (html == NULL) {
        return NULL;
    }
    KboWindowTextBuffer buffer;
    buffer.data = html;
    buffer.capacity = html_cap;
    buffer.length = 0;

    kbo_hub_ensure_valid_selection();

    char league_name[96] = {0};
    char team_name[96] = {0};
    char league_logo_path[MAX_PATH] = {0};
    char team_logo_path[MAX_PATH] = {0};
    char current_date_text[64] = {0};
    char window_status[256] = {0};
    kbo_hub_copy_league_display_name(g_kbo_hub_selected_league_id, league_name, sizeof(league_name));
    kbo_hub_copy_team_display_name_by_id(g_kbo_hub_selected_team_id, team_name, sizeof(team_name), "No team");
    kbo_get_foreign_waiver_window_status_text(window_status, sizeof(window_status));

    uint32_t current_year = 0, current_month = 0, current_day = 0;
    if (kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        kbo_hub_format_ootp_date(current_year, current_month, current_day, current_date_text, sizeof(current_date_text));
    } else {
        snprintf(current_date_text, sizeof(current_date_text), "DATE UNKNOWN");
    }
    kbo_hub_get_league_logo_path(g_kbo_hub_selected_league_id, current_year, league_logo_path, sizeof(league_logo_path));
    kbo_hub_get_team_logo_path(g_kbo_hub_selected_team_id, current_year, team_logo_path, sizeof(team_logo_path));

    kbo_window_text_appendf(&buffer,
        "<!doctype html><html><head><meta charset='utf-8'><style>"
        ":root{--bg:#0a0a0a;--header:#1d556c;--nav:#141414;--active:#1d556c;--panel:#161616;--panel2:#222;--ink:#fcfcfc;--muted:#8f8f8f;--orange:#de6d1f;--gold:#d6a44b;--line:rgba(255,255,255,.12)}"
        "*{box-sizing:border-box}html,body{height:100%;margin:0;overflow:hidden}body{background:var(--bg);color:var(--ink);font-family:'Malgun Gothic';font-size:%dpx}a{text-decoration:none;color:inherit}"
        ".app{height:100%;display:grid;grid-template-rows:64px 1fr;background:linear-gradient(135deg,#0a0a0a 0%%,#10171a 45%%,#080808 100%%)}"
        ".top{background:var(--header);display:flex;align-items:center;justify-content:space-between;padding:0 18px 0 18px;border-bottom:1px solid rgba(255,255,255,.18)}"
        ".identity{display:flex;align-items:center;gap:10px;min-width:0}.logo{width:46px;height:46px;object-fit:contain;filter:drop-shadow(0 1px 1px rgba(0,0,0,.65))}.brand{font-family:'Malgun Gothic';font-weight:800;font-size:%dpx;color:#f5f1e7;line-height:1}.date{font-family:'Malgun Gothic';font-size:%dpx;font-weight:800;color:#cfd5d6;margin-top:4px;text-transform:uppercase;letter-spacing:0}.brandBlock{min-width:0}"
        ".selects{display:flex;gap:10px}.select{min-width:162px;height:34px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 10px;border:1px solid rgba(255,255,255,.22);border-radius:4px;background:rgba(0,0,0,.16);color:#e6e6e8;font-family:'Malgun Gothic';font-weight:800}.select img{width:26px;height:26px;object-fit:contain;flex:none}.select span:first-of-type{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".dropdown{position:absolute;z-index:20;top:72px;max-height:420px;overflow-y:auto;overflow-x:hidden;background:#242424;border:1px solid #333;border-radius:3px;box-shadow:0 8px 18px rgba(0,0,0,.55);padding:4px 0;scrollbar-gutter:stable}.leagueMenu{right:190px;width:304px}.teamMenu{right:12px;width:304px}.ddItem{height:24px;display:flex!important;flex-direction:row!important;align-items:center;justify-content:flex-start;gap:6px;padding:0 10px 0 5px;color:#f2f2f2;font-family:'Malgun Gothic';font-size:16px;font-weight:800;line-height:24px;white-space:nowrap}.ddItem:hover,.ddItem.selected{background:#30434b}.ddLogo{width:20px;height:24px;display:inline-flex;align-items:center;justify-content:center;flex:0 0 20px;overflow:hidden}.ddLogo img{display:block;width:auto;height:auto;max-width:18px!important;max-height:18px!important;object-fit:contain}.ddText{display:block;flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dropdown::-webkit-scrollbar:vertical{width:20px}.dropdown::-webkit-scrollbar:horizontal{display:none;height:0}.dropdown::-webkit-scrollbar-track:vertical{background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_bar_mid.png') repeat-y center top}.dropdown::-webkit-scrollbar-thumb:vertical{min-height:42px;background:#2a2a2a url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_up_mid.png') repeat-y center top}.dropdown::-webkit-scrollbar-button:vertical:decrement{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_less_up.png') no-repeat center center}.dropdown::-webkit-scrollbar-button:vertical:increment{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_more_up.png') no-repeat center center}"
        ".shell{display:grid;grid-template-columns:196px 1fr;height:100%%;min-height:0;background:var(--nav)}.nav{background:var(--nav);padding:28px 20px 12px;border-right:1px solid #242424}.nav a{display:block;width:100%%;height:36px;margin-bottom:10px;line-height:36px;padding:0 14px;border:1px solid transparent;border-radius:4px;font-family:'Malgun Gothic';font-weight:800;letter-spacing:.03em}.nav a.active{background:var(--active);border-color:#343c40;border-left:5px solid var(--orange);padding-left:9px}"
        ".panel{margin:18px 16px 16px;background:var(--panel);border:1px solid var(--panel2);border-radius:5px;min-height:0;height:calc(100%% - 34px);display:grid;grid-template-rows:66px 1fr;overflow:hidden}.panelHead{background:var(--panel2);padding:10px 15px}.panelHead h1{margin:0;font-family:'Malgun Gothic';font-size:%dpx;font-weight:800}.panelHead p{margin:3px 0 0;color:var(--muted)}"
        ".content{padding:16px;min-height:0;height:100%%;display:flex;flex-direction:column;overflow:hidden}.content::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:20px}.content::-webkit-scrollbar:horizontal,.tablewrap::-webkit-scrollbar:horizontal,.card::-webkit-scrollbar:horizontal{display:none;height:0}.content::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_bar_mid.png') repeat-y center top}.content::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px;background:#2a2a2a url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_up_mid.png') repeat-y center top}.content::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_over_mid.png')}.content::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_less_up.png') no-repeat center center}.content::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_more_up.png') no-repeat center center}.content::-webkit-scrollbar-corner,.tablewrap::-webkit-scrollbar-corner,.card::-webkit-scrollbar-corner{background:#101010}.rights,.card{flex:1;min-height:0;height:100%%}.rights{display:flex;flex-direction:column;gap:10px}.card{overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010;padding:16px}.reportbar{display:none}.muted{color:var(--muted);line-height:1.42}.help{display:flex;gap:8px;align-items:center;color:#b9b9b9}.help span{background:#242424;border:1px solid #343434;border-radius:4px;padding:4px 8px}.actions{display:flex;gap:8px;justify-content:flex-end}.action{display:inline-block;min-width:128px;text-align:center;color:#f4f4f4;border:1px solid #3a3a3a;border-radius:4px;background:#262626;font-family:'Malgun Gothic';font-weight:800;padding:8px 12px}.keep{background:#8d4b17;border-color:#c46b22}.release{background:#1d556c;border-color:#2e7896}.toggle{width:52px;text-align:center}.switch{display:inline-block;width:18px;height:18px;line-height:16px;margin-right:4px;text-align:center;color:#aaa;border:1px solid #343434;border-radius:3px;background:#181818;font-family:'Segoe UI Symbol','Malgun Gothic';font-size:11px;font-weight:400;text-decoration:none}.switch:hover{color:#f0f0f0;border-color:#696969;background:#242424}.switch.keep,.switch.release{background:#181818;border-color:#343434}.tablewrap{flex:1;min-height:0;overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010}table{width:100%%;table-layout:fixed;border-collapse:separate;border-spacing:0;font-family:'Malgun Gothic';font-size:13px}th{position:sticky;top:0;background:#222;color:#dedede;text-align:left;padding:8px 10px;border-bottom:1px solid #393939;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}td{padding:7px 10px;border-bottom:1px solid #242424;color:#d8d8d8;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}thead th:first-child{border-top-left-radius:3px}thead th:last-child{border-top-right-radius:3px}tbody tr:last-child td:first-child{border-bottom-left-radius:3px}tbody tr:last-child td:last-child{border-bottom-right-radius:3px}tr:nth-child(even) td{background:#151515}tr:hover td,tr.selected td{background:#243b45;color:#fff}.sel{width:48px;color:var(--orange);font-weight:900;white-space:nowrap}.pname{width:156px;max-width:156px;overflow:hidden;text-overflow:ellipsis;font-weight:800;color:#f3f0e8}.team{width:64px;max-width:64px;text-align:left;font-weight:800;color:#ddd}.flag{width:44px;font-family:'Segoe UI Emoji','Apple Color Emoji','Noto Color Emoji','Malgun Gothic';font-size:18px;line-height:1;text-align:center;cursor:help}.empty{border:1px dashed rgba(255,255,255,.22);border-radius:4px;padding:22px;color:var(--muted);background:#141414;white-space:normal}"
        ".app{height:100vh}.shell{height:calc(100vh - 64px);align-items:stretch}.panel{height:auto!important;min-height:0!important;margin:18px 16px 16px;align-self:stretch;grid-template-rows:66px minmax(0,1fr)}.content{height:auto!important;min-height:0;overflow-y:auto!important;overflow-x:hidden!important;scrollbar-gutter:stable}.rights{height:100%%!important;min-height:0}.tablewrap{flex:1 1 auto;min-height:0;scrollbar-gutter:stable}.card{height:100%%!important;min-height:0;scrollbar-gutter:stable}"
        "</style></head><body><div class='app'><header class='top'><div class='identity'>",
        g_kbo_hub_skin_article_font_px - 1,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_button_font_px - 4,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_article_font_px - 2);
    if (league_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img class='logo' src='");
        kbo_webview_append_image_src(&buffer, league_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<div class='brandBlock'><div class='brand'>KBO FRONT OFFICE</div><div class='date'>");
    kbo_html_append_escaped(&buffer, current_date_text);
    kbo_window_text_appendf(&buffer, "</div></div></div><div class='selects'><a class='select' href='kbo://league'>");
    if (league_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img src='");
        kbo_webview_append_image_src(&buffer, league_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<span>");
    kbo_html_append_escaped(&buffer, league_name);
    kbo_window_text_appendf(&buffer, "</span><span>v</span></a><a class='select' href='kbo://team'>");
    if (team_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img src='");
        kbo_webview_append_image_src(&buffer, team_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<span>");
    kbo_html_append_escaped(&buffer, team_name);
    kbo_window_text_appendf(&buffer, "</span><span>v</span></a></div></header><div class='shell'><nav class='nav'>");
    if (g_kbo_hub_open_dropdown == 1) {
        kbo_webview_append_league_dropdown(&buffer, current_year);
    } else if (g_kbo_hub_open_dropdown == 2) {
        kbo_webview_append_team_dropdown(&buffer, current_year);
    }

    for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
        kbo_window_text_appendf(&buffer, "<a class='%s' href='kbo://view/%d'>",
            i == g_kbo_hub_selected_view ? "active" : "", i);
        kbo_html_append_escaped(&buffer, kbo_hub_nav_label(i));
        kbo_window_text_appendf(&buffer, "</a>");
    }

    kbo_window_text_appendf(&buffer, "</nav><main class='panel'><div class='panelHead'><h1>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_title());
    kbo_window_text_appendf(&buffer, "</h1><p>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_subtitle());
    kbo_window_text_appendf(&buffer, "</p></div><section class='content'>");

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
        uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
        uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
        char sang_name[64] = {0};
        char kpb_name[64]  = {0};
        kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "Sangmu Baseball Team");
        kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "Korean Police Baseball Team");

        int sang_due_60 = 0;
        int sang_due_now = 0;
        int kpb_due_60 = 0;
        int kpb_due_now = 0;
        int sang_count = kbo_hub_count_service_players(sang_id, &sang_due_60, &sang_due_now);
        int kpb_count  = kbo_hub_count_service_players(kpb_id,  &kpb_due_60,  &kpb_due_now);

        kbo_window_text_appendf(&buffer, "<div class='rights'><div class='reportbar'><div><b>KBO MILITARY SERVICE</b><div class='muted'>Tracked assignments: %ld. ",
            g_active_military_loan_count);
        kbo_html_append_escaped(&buffer, sang_name);
        kbo_window_text_appendf(&buffer, ": %d serving / %d due soon / %d ready. ", sang_count, sang_due_60, sang_due_now);
        kbo_html_append_escaped(&buffer, kpb_name);
        kbo_window_text_appendf(&buffer, ": %d serving / %d due soon / %d ready.</div></div><div class='help'><span>Original club</span><span>F5 refresh</span></div></div>",
            kpb_count, kpb_due_60, kpb_due_now);
        kbo_window_text_appendf(&buffer, "<section class='tablewrap'><table><thead><tr><th></th><th>PLAYER</th><th>SERVICE TEAM</th><th>ORIGINAL CLUB</th><th>RETURN DATE</th></tr></thead><tbody>");

        uintptr_t player_vector = 0;
        int32_t player_count = 0;
        int rendered = 0;
        if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
            for (int32_t i = 0; i < player_count && rendered < 500; i++) {
                uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
                if (!kbo_player_pointer_plausible(player_ptr)) { continue; }

                uint8_t* player = (uint8_t*)player_ptr;
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
                const char* service_fallback = NULL;
                const char* service_code = "";
                uint32_t service_team_id = 0;
                if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
                    service_team_id  = sang_id;
                    service_fallback = sang_name;
                    service_code     = "SANG";
                } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
                    service_team_id  = kpb_id;
                    service_fallback = kpb_name;
                    service_code     = "KPB";
                }
                if (service_team_id == 0) { continue; }

                uint32_t original_team_id = *(uint32_t*)(player + 0x5cu);
                int active_service_index = find_active_kbo_military_loan_index(player_id);
                if (active_service_index >= 0
                        && g_active_military_loans[active_service_index].original_team_id != 0u) {
                    original_team_id = g_active_military_loans[active_service_index].original_team_id;
                }
                if (g_kbo_hub_selected_team_id != 0 && original_team_id != g_kbo_hub_selected_team_id) {
                    continue;
                }

                char player_name[96] = {0};
                char service_team_name[64] = {0};
                char original_team_name[64] = {0};
                char return_date[16] = {0};
                kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
                kbo_hub_copy_team_display_name_by_id(service_team_id,  service_team_name,  sizeof(service_team_name),  service_fallback);
                kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);
                kbo_military_format_yyyymmdd(
                    kbo_military_effective_return_yyyymmdd(player),
                    return_date,
                    sizeof(return_date));

                kbo_window_text_appendf(&buffer, "<tr><td class='sel'>%s</td>", service_code);
                kbo_webview_append_player_name_cell(&buffer, player_name, player_id);
                kbo_window_text_appendf(&buffer, "<td>");
                kbo_html_append_escaped(&buffer, service_team_name);
                kbo_window_text_appendf(&buffer, "</td><td>");
                kbo_html_append_escaped(&buffer, original_team_name);
                kbo_window_text_appendf(&buffer, "</td><td>%s</td></tr>", return_date);
                rendered++;
            }
            if (rendered == 0) {
                kbo_window_text_appendf(&buffer, "<tr><td colspan='5'></td></tr>");
            }
        } else {
            kbo_window_text_appendf(&buffer, "<tr><td colspan='5'></td></tr>");
        }
        kbo_window_text_appendf(&buffer, "</tbody></table></section></div>");
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
        kbo_window_text_appendf(&buffer, "<div class='rights'><div class='reportbar'><div><b>KBO FOREIGN RESERVE RIGHTS</b><div class='muted'>");
        kbo_html_append_escaped(&buffer, window_status);
        kbo_window_text_appendf(&buffer, "</div></div><div class='help'><span>&#xD589;&#xC0AC;/&#xBBF8;&#xD589;&#xC0AC;</span><span>F5 refresh</span></div></div>");
        kbo_window_text_appendf(&buffer, "<section class='tablewrap'><table><thead><tr><th>RIGHTS</th><th>PLAYER</th><th>TEAM</th><th></th><th>STATUS</th></tr></thead><tbody>");

        uintptr_t player_vector = 0;
        int32_t player_count = 0;
        int rendered = 0;
        uint32_t top_player_id = 0;
        uint32_t top_current_team_id = 0;
        if (g_kbo_hub_selected_foreign_player_id == 0u
                && kbo_resolve_foreign_waiver_top_candidate_for_team(g_kbo_hub_selected_team_id, &top_player_id, &top_current_team_id)) {
            g_kbo_hub_selected_foreign_player_id = top_player_id;
        }
        if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
            for (int32_t i = 0; i < player_count && rendered < 500; i++) {
                uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
                if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
                uint8_t* player = (uint8_t*)player_ptr;
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
                if (player_id == 0u || decision_team_id != g_kbo_hub_selected_team_id
                        || !kbo_player_is_foreign_for_kbo_rights(player)) {
                    continue;
                }
                char flags[96] = {0};
                snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                    player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
                if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }
                uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
                kbo_webview_append_candidate_card(&buffer, player, player_id, current_team_id, nation_id, flags);
                rendered++;
            }
        }
        if (rendered == 0) {
            kbo_window_text_appendf(&buffer, "<tr><td colspan='5'></td></tr>");
        }
        kbo_window_text_appendf(&buffer, "</tbody></table></section></div>");
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        int configured_nations = kbo_load_asian_quota_nation_ids_once();
        uint32_t team_foreign = 0u;
        uint32_t team_asian = 0u;
        uint32_t team_non_asian = 0u;
        kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
        uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);
        kbo_window_text_appendf(&buffer, "<div class='rights'><div class='reportbar'><div><b>KBO FOREIGN PLAYER POLICY</b><div class='muted'>Policy: %s. Base effective limit: %u. Asian nation IDs: %d. Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u.</div></div><div class='help'><span>Selected team</span><span>Asian quota</span><span>F5 refresh</span></div></div>",
            kbo_custom_foreign_policy_enabled() ? "Custom KBO layer" : "OOTP fallback",
            KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
            configured_nations,
            team_foreign,
            team_asian,
            team_non_asian,
            team_effective);
        kbo_window_text_appendf(&buffer, "<section class='tablewrap'><table><thead><tr><th>SLOT</th><th>PLAYER</th><th>TEAM</th><th></th><th>STATUS</th></tr></thead><tbody>");

        uintptr_t player_vector = 0;
        int32_t player_count = 0;
        int rendered = 0;
        if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
            for (int32_t i = 0; i < player_count && rendered < 500; i++) {
                uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
                if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
                uint8_t* player = (uint8_t*)player_ptr;
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
                if (player_id == 0u
                        || !kbo_player_is_foreign_for_kbo_rights(player)
                        || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
                    continue;
                }

                char player_name[96] = {0};
                char current_team_abbrev[16] = {0};
                char flags[96] = {0};
                kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
                kbo_hub_copy_team_abbrev_by_id(current_team_id, current_team_abbrev, sizeof(current_team_abbrev), NULL);
                snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                    player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
                if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }

                kbo_window_text_appendf(&buffer, "<tr><td class='sel'>");
                kbo_html_append_escaped(&buffer, kbo_hub_foreign_slot_code_for_player(player));
                kbo_window_text_appendf(&buffer, "</td>");
                kbo_webview_append_player_name_cell(&buffer, player_name, player_id);
                kbo_webview_append_team_cell(&buffer, current_team_abbrev);
                kbo_webview_append_nation_flag_cell(&buffer, nation_id);
                kbo_window_text_appendf(&buffer, "<td>");
                kbo_html_append_escaped(&buffer, flags);
                kbo_window_text_appendf(&buffer, "</td></tr>");
                rendered++;
            }
        }
        if (rendered == 0) {
            kbo_window_text_appendf(&buffer, "<tr><td colspan='5'></td></tr>");
        }
        kbo_window_text_appendf(&buffer, "</tbody></table></section></div>");
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        kbo_clear_asian_games_roster_if_save_changed("hotkey_ui");
        LONG roster_count = g_kbo_asian_games_roster_count;
        if (roster_count <= 0) {
            kbo_load_asian_games_roster_csv("hotkey_ui");
            roster_count = g_kbo_asian_games_roster_count;
        }
        if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
            roster_count = 0;
        }
        int departed = 0;
        int returned = 0;
        int exempted = 0;
        for (LONG i = 0; i < roster_count; i++) {
            if (g_kbo_asian_games_roster[i].departed) { departed++; }
            if (g_kbo_asian_games_roster[i].returned) { returned++; }
            if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
        }
        kbo_window_text_appendf(&buffer, "<div class='rights'><div class='reportbar'><div><b>KBO ASIAN GAMES ROSTER</b><div class='muted'>Year: %u. Selected: %ld / Departed: %d / Returned: %d / Exempted: %d.</div></div><div class='help'><span>CSV roster</span><span>F5 refresh</span></div></div>",
            g_kbo_asian_games_roster_year, roster_count, departed, returned, exempted);
        kbo_window_text_appendf(&buffer, "<section class='tablewrap'><table><thead><tr><th></th><th>PLAYER</th><th>TEAM</th><th>AGE</th><th>ROLE</th><th>WC</th><th>STATUS</th></tr></thead><tbody>");
        if (roster_count == 0) {
            kbo_window_text_appendf(&buffer, "<tr><td colspan='7'></td></tr>");
        } else {
            for (LONG i = 0; i < roster_count; i++) {
                KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
                uintptr_t player_ptr = entry->player_ptr;
                if (!kbo_player_pointer_plausible(player_ptr)) {
                    player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
                }
                char player_name[96] = {0};
                if (kbo_player_pointer_plausible(player_ptr)) {
                    kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
                } else {
                    snprintf(player_name, sizeof(player_name), "Player #%u", entry->player_id);
                }
                char team_abbrev[16] = {0};
                kbo_hub_copy_team_abbrev_by_id(entry->original_team_id, team_abbrev, sizeof(team_abbrev), NULL);
                const char* status = "Selected";
                if (entry->returned) {
                    status = entry->exempted ? "Returned/Exempt" : "Returned";
                } else if (entry->departed) {
                    status = "Departed/IL";
                }
                kbo_window_text_appendf(&buffer, "<tr><td class='sel'>%02ld</td>", i + 1);
                kbo_webview_append_player_name_cell(&buffer, player_name, entry->player_id);
                kbo_webview_append_team_cell(&buffer, team_abbrev);
                kbo_window_text_appendf(&buffer, "<td>%u</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                    (uint32_t)entry->age,
                    kbo_asian_games_role_bucket_label(entry->role),
                    entry->wildcard ? "YES" : "NO",
                    status);
            }
        }
        kbo_window_text_appendf(&buffer, "</tbody></table></section></div>");
    } else {
        char text[32768] = {0};
        kbo_build_hub_window_text(text, sizeof(text));
        kbo_window_text_appendf(&buffer, "<div class='card'><pre style=\"white-space:pre-wrap;margin:0;font-family:'Malgun Gothic';line-height:1.55\">");
        kbo_html_append_escaped(&buffer, text);
        kbo_window_text_appendf(&buffer, "</pre></div>");
    }

    kbo_window_text_appendf(&buffer, "</section></main></div></div></body></html>");

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, html, -1, NULL, 0);
    WCHAR* wide = NULL;
    if (wide_len > 0) {
        wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            MultiByteToWideChar(CP_UTF8, 0, html, -1, wide, wide_len);
        }
    }
    HeapFree(GetProcessHeap(), 0, html);
    return wide;
}

static void kbo_webview_navigate_current(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }
    WCHAR* html = kbo_build_webview_hub_html();
    if (html != NULL) {
        ICoreWebView2_NavigateToString(g_kbo_webview, html);
        HeapFree(GetProcessHeap(), 0, html);
    }
}

typedef HRESULT (STDAPICALLTYPE *KboCreateCoreWebView2EnvironmentWithOptionsFn)(
    PCWSTR,
    PCWSTR,
    ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

typedef struct KboWebViewEnvHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewEnvHandler;

typedef struct KboWebViewControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewControllerHandler;

typedef struct KboWebViewNavHandler {
    ICoreWebView2NavigationStartingEventHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewNavHandler;

static HRESULT STDMETHODCALLTYPE kbo_webview_nav_qi(ICoreWebView2NavigationStartingEventHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2NavigationStartingEventHandler)) {
        *ppv = This;
        ICoreWebView2NavigationStartingEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_addref(ICoreWebView2NavigationStartingEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewNavHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_release(ICoreWebView2NavigationStartingEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewNavHandler*)This)->ref);
    if (value < 1) { ((KboWebViewNavHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewNavHandler*)This)->ref;
}

static int kbo_webview_handle_command_uri(const char* uri, HWND hwnd)
{
    if (uri == NULL || strncmp(uri, "kbo://", 6) != 0) {
        return 0;
    }
    const char* cmd = uri + 6;
    if (strncmp(cmd, "select/", 7) == 0) {
        uint32_t player_id = (uint32_t)strtoul(cmd + 7, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            append_logf("foreign rights webview: selected player=%u", player_id);
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "retain/", 7) == 0 || strncmp(cmd, "release/", 8) == 0) {
        int retain = strncmp(cmd, "retain/", 7) == 0;
        const char* id_text = retain ? cmd + 7 : cmd + 8;
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            kbo_apply_foreign_rights_button(retain);
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "view/", 5) == 0) {
        int view = atoi(cmd + 5);
        if (view >= 0 && view < KBO_HUB_NAV_COUNT) {
            g_kbo_hub_selected_view = view;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "player/", 7) == 0) {
        uint32_t player_id = (uint32_t)strtoul(cmd + 7, NULL, 10);
        if (player_id != 0u) {
            append_logf("KBO WebView player link clicked player=%u", player_id);
            kbo_open_ootp_player_profile_async(player_id);
        }
        return 1;
    }
    if (strncmp(cmd, "setleague/", 10) == 0) {
        uint32_t league_id = (uint32_t)strtoul(cmd + 10, NULL, 10);
        if (league_id != 0u) {
            g_kbo_hub_selected_league_id = league_id;
            g_kbo_hub_selected_team_id = 0;
            g_kbo_hub_open_dropdown = 0;
            kbo_hub_ensure_valid_selection();
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "setteam/", 8) == 0) {
        uint32_t team_id = (uint32_t)strtoul(cmd + 8, NULL, 10);
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "league/") == 0 || strcmp(cmd, "league") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 1 ? 0 : 1;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "team/") == 0 || strcmp(cmd, "team") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 2 ? 0 : 2;
        kbo_webview_navigate_current();
        return 1;
    }
    return 1;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args)
{
    (void)sender;
    KboWebViewNavHandler* handler = (KboWebViewNavHandler*)This;
    LPWSTR uri_w = NULL;
    if (args == NULL || FAILED(ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &uri_w)) || uri_w == NULL) {
        return S_OK;
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, NULL, 0, NULL, NULL);
    char uri[512] = {0};
    if (needed > 0) {
        WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, uri, sizeof(uri), NULL, NULL);
    }
    CoTaskMemFree(uri_w);
    if (kbo_webview_handle_command_uri(uri, handler->hwnd)) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    }
    return S_OK;
}

static const ICoreWebView2NavigationStartingEventHandlerVtbl g_kbo_webview_nav_vtbl = {
    kbo_webview_nav_qi,
    kbo_webview_nav_addref,
    kbo_webview_nav_release,
    kbo_webview_nav_invoke
};

static KboWebViewNavHandler g_kbo_webview_nav_handler = {
    { &g_kbo_webview_nav_vtbl },
    1,
    NULL
};

static EventRegistrationToken g_kbo_webview_nav_token = {0};

static HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewEnvHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewEnvHandler*)This)->ref);
    if (value < 1) { ((KboWebViewEnvHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewEnvHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewControllerHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewControllerHandler*)This)->ref);
    if (value < 1) { ((KboWebViewControllerHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewControllerHandler*)This)->ref;
}

static void kbo_webview_set_bounds(HWND hwnd)
{
    if (hwnd == NULL || g_kbo_webview_controller == NULL) {
        return;
    }
    RECT client;
    GetClientRect(hwnd, &client);
    RECT bounds = {0, 0, client.right, client.bottom};
    ICoreWebView2Controller_put_Bounds(g_kbo_webview_controller, bounds);
    ICoreWebView2Controller_put_IsVisible(g_kbo_webview_controller, TRUE);
}

static void kbo_webview_apply_ootp_like_settings(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }

    ICoreWebView2Settings* settings = NULL;
    if (FAILED(ICoreWebView2_get_Settings(g_kbo_webview, &settings)) || settings == NULL) {
        return;
    }

    ICoreWebView2Settings_put_IsStatusBarEnabled(settings, FALSE);
    ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, FALSE);
    ICoreWebView2Settings_put_AreDevToolsEnabled(settings, FALSE);
    ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
    ICoreWebView2Settings_Release(settings);
}

static HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result)
{
    KboWebViewControllerHandler* handler = (KboWebViewControllerHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        append_logf("WebView2 controller create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller = result;
    ICoreWebView2Controller_AddRef(g_kbo_webview_controller);
    ICoreWebView2Controller_get_CoreWebView2(g_kbo_webview_controller, &g_kbo_webview);
    if (g_kbo_webview != NULL) {
        kbo_webview_apply_ootp_like_settings();
        g_kbo_webview_nav_handler.hwnd = handler->hwnd;
        ICoreWebView2_add_NavigationStarting(g_kbo_webview, &g_kbo_webview_nav_handler.iface, &g_kbo_webview_nav_token);
        kbo_webview_navigate_current();
    }
    InterlockedExchange(&g_kbo_webview_ready, 1);
    kbo_webview_set_bounds(handler->hwnd);
    append_log_line("WebView2 F2 rights UI ready");
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_kbo_webview_controller_vtbl = {
    kbo_webview_controller_qi,
    kbo_webview_controller_addref,
    kbo_webview_controller_release,
    kbo_webview_controller_invoke
};

static KboWebViewControllerHandler g_kbo_webview_controller_handler = {
    { &g_kbo_webview_controller_vtbl },
    1,
    NULL
};

static HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Environment* result)
{
    KboWebViewEnvHandler* handler = (KboWebViewEnvHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        append_logf("WebView2 environment create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller_handler.hwnd = handler->hwnd;
    HRESULT hr = ICoreWebView2Environment_CreateCoreWebView2Controller(
        result,
        handler->hwnd,
        &g_kbo_webview_controller_handler.iface);
    if (FAILED(hr)) {
        append_logf("WebView2 CreateCoreWebView2Controller failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    }
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_kbo_webview_env_vtbl = {
    kbo_webview_env_qi,
    kbo_webview_env_addref,
    kbo_webview_env_release,
    kbo_webview_env_invoke
};

static KboWebViewEnvHandler g_kbo_webview_env_handler = {
    { &g_kbo_webview_env_vtbl },
    1,
    NULL
};

static void kbo_start_webview_rights_ui(HWND hwnd)
{
    if (hwnd == NULL || InterlockedCompareExchange(&g_kbo_webview_starting, 1, 0) != 0) {
        kbo_webview_set_bounds(hwnd);
        return;
    }

    HMODULE self_module = NULL;
    char loader_path[MAX_PATH] = {0};
    HMODULE loader = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_start_webview_rights_ui, &self_module)
            && GetModuleFileNameA(self_module, loader_path, sizeof(loader_path)) > 0) {
        char* slash = strrchr(loader_path, '\\');
        if (slash != NULL) {
            slash[1] = '\0';
            strncat(loader_path, "WebView2Loader.dll", sizeof(loader_path) - strlen(loader_path) - 1);
            loader = LoadLibraryA(loader_path);
        }
    }
    if (loader == NULL) {
        loader = LoadLibraryA("WebView2Loader.dll");
    }
    if (loader == NULL) {
        append_logf("WebView2Loader.dll load failed error=%lu", GetLastError());
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    KboCreateCoreWebView2EnvironmentWithOptionsFn create_env =
        (KboCreateCoreWebView2EnvironmentWithOptionsFn)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (create_env == NULL) {
        append_log_line("WebView2 CreateCoreWebView2EnvironmentWithOptions missing");
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    WCHAR user_data[MAX_PATH] = {0};
    WCHAR local[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local, (DWORD)(sizeof(local) / sizeof(local[0])));
    if (got > 0 && got < (DWORD)(sizeof(local) / sizeof(local[0]))) {
        swprintf(user_data, sizeof(user_data) / sizeof(user_data[0]), L"%ls\\OOTP-KBO\\WebView2", local);
        CreateDirectoryW(user_data, NULL);
    }

    g_kbo_webview_env_handler.hwnd = hwnd;
    HRESULT hr = create_env(NULL, user_data[0] != L'\0' ? user_data : NULL, NULL, &g_kbo_webview_env_handler.iface);
    if (FAILED(hr)) {
        append_logf("WebView2 environment start failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    } else {
        append_log_line("WebView2 F2 rights UI starting");
    }
}

static void kbo_layout_hotkey_window(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_start_webview_rights_ui(hwnd);
    kbo_webview_set_bounds(hwnd);
}

static void kbo_refresh_hotkey_window_layout(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_layout_hotkey_window(hwnd);
    kbo_refresh_hotkey_window();
    if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) != 0) {
        kbo_webview_navigate_current();
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static RECT kbo_hub_fixed_window_rect(void)
{
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE;
    RECT rect = {0, 0, KBO_HUB_FIXED_CLIENT_WIDTH, KBO_HUB_FIXED_CLIENT_HEIGHT};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    return rect;
}

static void kbo_hub_apply_fixed_window_size(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    RECT rect = kbo_hub_fixed_window_rect();
    SetWindowPos(
        hwnd,
        NULL,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        kbo_hub_load_language_setting();
        kbo_hub_init_gdi_objects();
        kbo_hub_ensure_valid_selection();
        SetTimer(hwnd, 1u, 80u, NULL);
        kbo_hub_apply_fixed_window_size(hwnd);
        kbo_layout_hotkey_window(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        kbo_layout_hotkey_window(hwnd);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc != NULL) {
            RECT client;
            GetClientRect(hwnd, &client);
            FillRect(hdc, &client, g_kbo_hub_brush_bg);
            if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) == 0) {
                RECT text_rect = {22, 22, client.right - 22, 70};
                kbo_hub_draw_text(hdc, "KBO FRONT OFFICE HTML UI LOADING...", text_rect,
                    RGB(245, 241, 231), g_kbo_hub_font_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            kbo_hub_apply_fixed_window_size(hwnd);
        }
        kbo_refresh_hotkey_window_layout(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = (MINMAXINFO*)lparam;
        if (info != NULL) {
            RECT rect = kbo_hub_fixed_window_rect();
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            info->ptMinTrackSize.x = width;
            info->ptMinTrackSize.y = height;
            info->ptMaxTrackSize.x = width;
            info->ptMaxTrackSize.y = height;
            info->ptMaxSize.x = width;
            info->ptMaxSize.y = height;
        }
        return 0;
    }

    case WM_COMMAND:
        break;

    case WM_LBUTTONDOWN: {
        POINT point;
        point.x = (int)(short)LOWORD(lparam);
        point.y = (int)(short)HIWORD(lparam);
        if (PtInRect(&g_kbo_hub_league_dropdown_rect, point)) {
            kbo_hub_show_league_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_team_dropdown_rect, point)) {
            kbo_hub_show_team_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_ko_rect, point)) {
            g_kbo_hub_language = KBO_HUB_LANG_KO;
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_en_rect, point)) {
            g_kbo_hub_language = KBO_HUB_LANG_EN;
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_less_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, -1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_more_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, 1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                int direction = point.y < g_kbo_hub_scrollbar_thumb_rect.top
                    ? -kbo_hub_estimate_visible_edit_lines()
                    :  kbo_hub_estimate_visible_edit_lines();
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, direction);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        RECT wnd_rect;
        GetClientRect(hwnd, &wnd_rect);
        int wnd_w = wnd_rect.right - wnd_rect.left;
        int wnd_h = wnd_rect.bottom - wnd_rect.top;
        for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
            RECT item = kbo_hub_nav_item_rect(i, wnd_w, wnd_h);
            if (PtInRect(&item, point)) {
                g_kbo_hub_selected_view = i;
                kbo_refresh_hotkey_window_layout(hwnd);
                return 0;
            }
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wparam;
        SetTextColor(hdc, KBO_HUB_COLOR_TEXT);
        SetBkColor(hdc, KBO_HUB_COLOR_PANEL_ALT);
        return (LRESULT)g_kbo_hub_brush_panel_alt;
    }

    case WM_TIMER:
        if (wparam == 1u
                && g_kbo_hotkey_keyboard_hook == NULL
                && (GetAsyncKeyState(VK_F2) & 1) != 0
                && kbo_foreground_is_this_process()) {
            kbo_show_or_hide_hotkey_window();
            return 0;
        }
        if (wparam == 1u
                && g_kbo_hotkey_keyboard_hook == NULL
                && (GetAsyncKeyState(VK_F5) & 1) != 0
                && g_kbo_hotkey_window != NULL
                && IsWindowVisible(g_kbo_hotkey_window)
                && kbo_foreground_is_this_process()) {
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wparam == 1u && g_kbo_hotkey_window != NULL && IsWindowVisible(g_kbo_hotkey_window)) {
            static uintptr_t s_last_db_ptr = 0;
            uintptr_t cur_db = g_ootp_cached_global_db;
            if (cur_db != s_last_db_ptr) {
                s_last_db_ptr = cur_db;
                kbo_refresh_hotkey_window();
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
        }
        kbo_try_install_game_getmsg_hook();
        break;

    case KBO_WM_TOGGLE_SERVICE_MONITOR:
        kbo_show_or_hide_hotkey_window();
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1u);
        if (g_kbo_hotkey_keyboard_hook != NULL) {
            UnhookWindowsHookEx(g_kbo_hotkey_keyboard_hook);
            g_kbo_hotkey_keyboard_hook = NULL;
        }
        if (g_kbo_game_getmsg_hook != NULL) {
            UnhookWindowsHookEx(g_kbo_game_getmsg_hook);
            g_kbo_game_getmsg_hook = NULL;
        }
        g_kbo_hotkey_window         = NULL;
        g_kbo_hotkey_edit           = NULL;
        g_kbo_foreign_list          = NULL;
        g_kbo_foreign_keep_button   = NULL;
        g_kbo_foreign_release_button = NULL;
        g_kbo_hotkey_edit_original_proc = NULL;
        kbo_hub_delete_gdi_objects();
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter)
{
    g_kbo_hotkey_instance = (HINSTANCE)parameter;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    kbo_hub_init_gdi_objects();

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = kbo_hotkey_window_proc;
    wc.hInstance     = g_kbo_hotkey_instance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = g_kbo_hub_brush_bg;
    wc.lpszClassName = "OOTPKBOHubWindow";

    ATOM klass = RegisterClassExA(&wc);
    if (klass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        append_logf("KBO F2 hub class registration failed error=%lu", GetLastError());
        return 0;
    }

    HWND owner = kbo_find_ootp_main_window();
    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD window_ex_style = WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE;
    RECT fixed_rect = kbo_hub_fixed_window_rect();
    HWND hwnd = CreateWindowExA(
        window_ex_style,
        wc.lpszClassName,
        "KBO Companion",
        window_style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        fixed_rect.right - fixed_rect.left,
        fixed_rect.bottom - fixed_rect.top,
        owner, NULL, g_kbo_hotkey_instance, NULL);

    if (hwnd == NULL) {
        append_logf("KBO F2 hub window creation failed error=%lu", GetLastError());
        return 0;
    }

    g_kbo_hotkey_window = hwnd;
    g_kbo_hotkey_keyboard_hook = SetWindowsHookExA(
        WH_KEYBOARD_LL, kbo_hotkey_keyboard_proc, g_kbo_hotkey_instance, 0);
    if (g_kbo_hotkey_keyboard_hook == NULL) {
        append_logf("KBO F2 hub keyboard hook failed error=%lu; falling back to polling", GetLastError());
    } else {
        append_logf("KBO F2 hub keyboard hook installed hook=%p", (void*)g_kbo_hotkey_keyboard_hook);
    }
    append_logf("KBO F2 hub ready hwnd=%p owner=%p", (void*)hwnd, (void*)owner);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    CoUninitialize();
    return 0;
}

static void start_kbo_hotkey_window_thread(HINSTANCE instance)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_hotkey_window_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_hotkey_window_thread, instance, 0, &g_kbo_hotkey_thread_id);
    if (thread == NULL) {
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        append_logf("KBO F2 hub thread failed error=%lu", GetLastError());
        return;
    }

    CloseHandle(thread);
}
