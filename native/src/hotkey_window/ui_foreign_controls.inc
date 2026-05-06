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


