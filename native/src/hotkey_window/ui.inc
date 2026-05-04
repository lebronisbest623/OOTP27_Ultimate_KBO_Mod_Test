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
    case KBO_HUB_VIEW_MOD_INFO: return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",      "SETTINGS");
    case KBO_HUB_VIEW_OVERVIEW:
    default:                    return kbo_hub_text("\xed\x99\x88",        "HOME");
    }
}

static const char* kbo_hub_current_view_subtitle(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: return kbo_hub_text("\xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98\xec\x99\x80 \xeb\xb3\xb5\xea\xb7\x80 \xec\x8b\x9c\xec\xa0\x90\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",   "Who is serving and when they return");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xed\x8c\x80\xeb\xb3\x84 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4", "Retained foreign player rights by club");
    case KBO_HUB_VIEW_MOD_INFO: return kbo_hub_text("\xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xeb\x9f\xb0\xed\x83\x80\xec\x9e\x84 \xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c\xeb\xa5\xbc \xed\x91\x9c\xec\x8b\x9c\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",       "Launcher and runtime patch status");
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xec\x96\xb8\xec\x96\xb4\xec\x99\x80 \xeb\x8b\xa8\xec\xb6\x95\xed\x82\xa4\xeb\xa5\xbc \xec\x84\xa4\xec\xa0\x95\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",                "Language and hotkey settings");
    case KBO_HUB_VIEW_OVERVIEW:
    default:                    return kbo_hub_text("\xec\x9d\xb4 \xec\x84\xb8\xec\x9d\xb4\xeb\xb8\x8c\xec\x9d\x98 \xed\x95\xb5\xec\x8b\xac \xec\x83\x81\xed\x83\x9c\xeb\xa5\xbc \xed\x95\x9c \xeb\xb2\x88\xec\x97\x90 \xeb\xb4\x85\xeb\x8b\x88\xeb\x8b\xa4",    "Quick summary of this save");
    }
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

    POINT pt = {g_kbo_hub_league_dropdown_rect.left, g_kbo_hub_league_dropdown_rect.bottom};
    ClientToScreen(hwnd, &pt);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= 1000u && command < 1000u + (UINT)league_count) {
        g_kbo_hub_selected_league_id = leagues[command - 1000u];
        g_kbo_hub_selected_team_id   = 0;
        kbo_hub_ensure_valid_selection();
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
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

    POINT pt = {g_kbo_hub_team_dropdown_rect.left, g_kbo_hub_team_dropdown_rect.bottom};
    ClientToScreen(hwnd, &pt);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= 2000u && command < 2000u + (UINT)filtered_count) {
        g_kbo_hub_selected_team_id = teams[command - 2000u];
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
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
        if (player_id == 0u || (current_team_id != g_kbo_hub_selected_team_id && active_team_id != g_kbo_hub_selected_team_id)
                || !kbo_player_is_foreign_for_kbo_rights(player)) {
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

static void kbo_webview_append_candidate_card(
    KboWindowTextBuffer* buffer,
    uint8_t* player,
    uint32_t player_id,
    uint32_t current_team_id,
    uint32_t nation_id,
    const char* flags,
    int selected)
{
    char player_name[96] = {0};
    int value_score = kbo_foreign_waiver_value_score(player);
    kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
    kbo_window_text_appendf(buffer,
        "<tr class='%s' onclick=\"location.href='kbo://select/%u'\"><td class='sel'>%s</td><td class='pname'>",
        selected ? "selected" : "",
        player_id,
        selected ? "&gt;" : "");
    kbo_html_append_escaped(buffer, player_name);
    kbo_window_text_appendf(buffer, "</td><td>%u</td><td>%u</td><td>%u</td><td>%d</td><td>",
        player_id, current_team_id, nation_id, value_score);
    kbo_html_append_escaped(buffer, flags != NULL && flags[0] != '\0' ? flags : "Active");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static WCHAR* kbo_build_webview_hub_html(void)
{
    const size_t html_cap = 196608;
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
    char current_date_text[64] = {0};
    char window_status[256] = {0};
    kbo_hub_copy_league_display_name(g_kbo_hub_selected_league_id, league_name, sizeof(league_name));
    kbo_hub_copy_team_display_name_by_id(g_kbo_hub_selected_team_id, team_name, sizeof(team_name), "No team");
    kbo_get_foreign_waiver_window_status_text(window_status, sizeof(window_status));

    uint32_t current_year = 0, current_month = 0, current_day = 0;
    if (kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        snprintf(current_date_text, sizeof(current_date_text), "DATE %04u-%02u-%02u", current_year, current_month, current_day);
    } else {
        snprintf(current_date_text, sizeof(current_date_text), "DATE UNKNOWN");
    }

    kbo_window_text_appendf(&buffer,
        "<!doctype html><html><head><meta charset='utf-8'><style>"
        ":root{--bg:#0a0a0a;--header:#1d556c;--nav:#141414;--active:#1d556c;--panel:#161616;--panel2:#222;--ink:#fcfcfc;--muted:#8f8f8f;--orange:#de6d1f;--gold:#d6a44b;--line:rgba(255,255,255,.12)}"
        "*{box-sizing:border-box}html,body{height:100%;margin:0;overflow:hidden}body{background:var(--bg);color:var(--ink);font-family:'Malgun Gothic';font-size:%dpx}a{text-decoration:none;color:inherit}"
        ".app{height:100%;display:grid;grid-template-rows:58px 1fr;background:linear-gradient(135deg,#0a0a0a 0%%,#10171a 45%%,#080808 100%%)}"
        ".top{background:var(--header);display:flex;align-items:center;justify-content:space-between;padding:0 18px 0 22px;border-bottom:1px solid rgba(255,255,255,.18)}"
        ".brand{font-family:'Malgun Gothic';font-weight:800;font-size:%dpx;color:#f5f1e7}.date{font-family:'Malgun Gothic';font-size:%dpx;font-weight:800;color:#c7d1d5;margin-top:2px}"
        ".selects{display:flex;gap:10px}.select{min-width:162px;height:32px;display:flex;align-items:center;justify-content:space-between;padding:0 10px;border:1px solid rgba(255,255,255,.22);background:rgba(0,0,0,.16);color:#e6e6e8;font-family:'Malgun Gothic';font-weight:800}"
        ".shell{display:grid;grid-template-columns:196px 1fr;height:100%%;min-height:0}.nav{background:var(--nav);padding:40px 20px 12px;border-right:1px solid #242424}.nav a{display:block;width:100%%;height:36px;margin-bottom:10px;line-height:36px;padding:0 14px;border:1px solid transparent;font-family:'Malgun Gothic';font-weight:800;letter-spacing:.03em}.nav a.active{background:var(--active);border-color:#343c40;border-left:5px solid var(--orange);padding-left:9px}"
        ".panel{margin:18px 16px 16px;background:var(--panel);border:1px solid var(--panel2);min-height:0;display:grid;grid-template-rows:66px 1fr}.panelHead{background:var(--panel2);padding:10px 15px}.panelHead h1{margin:0;font-family:'Malgun Gothic';font-size:%dpx;font-weight:800}.panelHead p{margin:3px 0 0;color:var(--muted)}"
        ".content{padding:16px;min-height:0;overflow:hidden}.rights{height:100%%;display:grid;grid-template-rows:auto auto 1fr;gap:10px}.reportbar{display:grid;grid-template-columns:1fr auto;gap:12px;background:#1b1b1b;border:1px solid #2f2f2f;border-left:5px solid var(--orange);padding:10px 12px}.reportbar b{display:block;font-family:'Malgun Gothic';font-size:%dpx;color:#f3f0e8}.muted{color:var(--muted);line-height:1.42}.help{display:flex;gap:8px;align-items:center;color:#b9b9b9}.help span{background:#242424;border:1px solid #343434;padding:4px 8px}.actions{display:flex;gap:8px;justify-content:flex-end}.action{display:inline-block;min-width:128px;text-align:center;color:#f4f4f4;border:1px solid #3a3a3a;background:#262626;font-family:'Malgun Gothic';font-weight:800;padding:8px 12px}.keep{background:#8d4b17;border-color:#c46b22}.release{background:#1d556c;border-color:#2e7896}.tablewrap{min-height:0;overflow:auto;border:1px solid #303030;background:#101010}table{width:100%%;border-collapse:collapse;font-family:'Malgun Gothic';font-size:13px}th{position:sticky;top:0;background:#222;color:#dedede;text-align:left;padding:8px 10px;border-bottom:1px solid #393939}td{padding:7px 10px;border-bottom:1px solid #242424;color:#d8d8d8}tr:nth-child(even) td{background:#151515}tr:hover td,tr.selected td{background:#243b45;color:#fff}.sel{width:26px;color:var(--orange);font-weight:900}.pname{font-weight:800;color:#f3f0e8}.empty{border:1px dashed rgba(255,255,255,.22);padding:22px;color:var(--muted);background:#141414}"
        "</style></head><body><div class='app'><header class='top'><div><div class='brand'>KBO FRONT OFFICE</div><div class='date'>",
        g_kbo_hub_skin_article_font_px - 1,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_button_font_px - 4,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_article_font_px - 2);
    kbo_html_append_escaped(&buffer, current_date_text);
    kbo_window_text_appendf(&buffer, "</div></div><div class='selects'><a class='select' href='kbo://league'>");
    kbo_html_append_escaped(&buffer, league_name);
    kbo_window_text_appendf(&buffer, " <span>v</span></a><a class='select' href='kbo://team'>");
    kbo_html_append_escaped(&buffer, team_name);
    kbo_window_text_appendf(&buffer, " <span>v</span></a></div></header><div class='shell'><nav class='nav'>");

    const char* nav_labels[KBO_HUB_NAV_COUNT] = {"HOME", "SERVICE", "RIGHTS", "MOD INFO", "SETTINGS"};
    const char* nav_keys[KBO_HUB_NAV_COUNT] = {"home", "service", "rights", "mod", "settings"};
    for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
        kbo_window_text_appendf(&buffer, "<a class='%s' href='kbo://view/%d'>%s</a>",
            i == g_kbo_hub_selected_view ? "active" : "", i, nav_labels[i]);
        (void)nav_keys;
    }

    kbo_window_text_appendf(&buffer, "</nav><main class='panel'><div class='panelHead'><h1>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_title());
    kbo_window_text_appendf(&buffer, "</h1><p>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_subtitle());
    kbo_window_text_appendf(&buffer, "</p></div><section class='content'>");

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
        kbo_window_text_appendf(&buffer, "<div class='rights'><div class='reportbar'><div><b>KBO FOREIGN RESERVE RIGHTS</b><div class='muted'>");
        kbo_html_append_escaped(&buffer, window_status);
        kbo_window_text_appendf(&buffer, "</div></div><div class='help'><span>1. Select player</span><span>2. Keep or release</span></div></div>");
        if (g_kbo_hub_selected_foreign_player_id != 0u) {
            kbo_window_text_appendf(&buffer, "<div class='actions'><a class='action keep' href='kbo://retain/%u'>KEEP RIGHTS</a><a class='action release' href='kbo://release/%u'>RELEASE RIGHTS</a></div>",
                g_kbo_hub_selected_foreign_player_id, g_kbo_hub_selected_foreign_player_id);
        } else {
            kbo_window_text_appendf(&buffer, "<div class='actions'><span class='action'>NO PLAYER SELECTED</span></div>");
        }
        kbo_window_text_appendf(&buffer, "<section class='tablewrap'><table><thead><tr><th></th><th>PLAYER</th><th>ID</th><th>TEAM</th><th>NAT</th><th>VALUE</th><th>STATUS</th></tr></thead><tbody>");

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
                kbo_webview_append_candidate_card(&buffer, player, player_id, current_team_id, nation_id, flags,
                    player_id == g_kbo_hub_selected_foreign_player_id);
                rendered++;
            }
        }
        if (rendered == 0) {
            kbo_window_text_appendf(&buffer, "<tr><td colspan='7'><div class='empty'>No reserve-rights candidates found for the selected team.</div></td></tr>");
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
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "league/") == 0 || strcmp(cmd, "league") == 0) {
        kbo_hub_show_league_dropdown(hwnd);
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "team/") == 0 || strcmp(cmd, "team") == 0) {
        kbo_hub_show_team_dropdown(hwnd);
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

static LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        kbo_hub_load_language_setting();
        kbo_hub_init_gdi_objects();
        kbo_hub_ensure_valid_selection();
        SetTimer(hwnd, 1u, 80u, NULL);
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
        kbo_refresh_hotkey_window_layout(hwnd);
        return 0;

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
    HWND hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE,
        wc.lpszClassName,
        "KBO Companion",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1040, 720,
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
