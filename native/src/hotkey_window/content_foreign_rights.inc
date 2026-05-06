static void kbo_build_foreign_rights_window_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint32_t selected_team_id = g_kbo_hub_selected_team_id;
    char selected_team_name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(selected_team_id, selected_team_name, sizeof(selected_team_name), NULL);

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "FOREIGN PLAYER RIGHTS"));
    kbo_window_text_appendf(&buffer, "%s: %s (%u)\r\n", kbo_hub_text("\xed\x8c\x80", "Team"), selected_team_name, selected_team_id);
    char window_status[256] = {0};
    kbo_get_foreign_waiver_window_status_text(window_status, sizeof(window_status));
    kbo_window_text_appendf(&buffer, "%s\r\n", window_status);
    kbo_window_text_appendf(&buffer, "\r\n");
    kbo_window_text_appendf(&buffer, "HOW TO USE\r\n");
    kbo_window_text_appendf(&buffer, "  1. Choose a team from the top-right team dropdown.\r\n");
    kbo_window_text_appendf(&buffer, "  2. Select a foreign player in the list.\r\n");
    kbo_window_text_appendf(&buffer, "  3. Click KEEP RIGHTS or RELEASE RIGHTS.\r\n\r\n");

    uint32_t top_player_id = 0;
    uint32_t top_current_team_id = 0;
    if (kbo_resolve_foreign_waiver_top_candidate_for_team(selected_team_id, &top_player_id, &top_current_team_id)) {
        char top_player_name[64] = {0};
        uint8_t* top_player = kbo_find_player_by_id(top_player_id, NULL, NULL);
        if (top_player != NULL) {
            kbo_hub_copy_player_display_name(top_player, top_player_name, sizeof(top_player_name));
        }
        if (g_kbo_hub_selected_foreign_player_id == 0u) {
            g_kbo_hub_selected_foreign_player_id = top_player_id;
        }
        if (g_kbo_hub_selected_foreign_player_id == top_player_id) {
            kbo_window_text_appendf(&buffer, "SELECTED: %s (%u)\r\n\r\n", top_player_name, top_player_id);
        } else {
            char selected_name[64] = {0};
            uint8_t* selected_player = kbo_find_player_by_id(g_kbo_hub_selected_foreign_player_id, NULL, NULL);
            if (selected_player != NULL) {
                kbo_hub_copy_player_display_name(selected_player, selected_name, sizeof(selected_name));
                kbo_window_text_appendf(&buffer, "SELECTED: %s (%u)\r\n\r\n", selected_name, g_kbo_hub_selected_foreign_player_id);
            } else {
                kbo_window_text_appendf(&buffer, "SELECTED: %s (%u)\r\n\r\n", top_player_name, top_player_id);
            }
        }
    } else {
        kbo_window_text_appendf(&buffer, "SELECTED: (none)\r\n\r\n");
    }

    if (selected_team_id == 0) {
        kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xed\x8c\x80\xec\x9d\x84 \xeb\xa8\xbc\xec\xa0\x80 \xec\x84\xa0\xed\x83\x9d\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.", "Select a team first."));
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "FOREIGN PLAYERS ON THIS TEAM\r\n");
    kbo_window_text_appendf(&buffer, "  PLAYER                   ID          TEAM       NAT     STATUS\r\n");
    kbo_window_text_appendf(&buffer, "--------------------------------------------------------------------------\r\n");

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int forced = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        (void)forced;
        (void)score;
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        if (player_id == 0 || decision_team_id != selected_team_id
                || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        char player_name[64] = {0};
        char flags[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        snprintf(flags, sizeof(flags), "%s%s%s%s%s",
            restricted ? "Restricted " : "",
            secondary ? "SecRestricted " : "",
            dfa ? "DFA " : "",
            loan_active ? "Loan " : "",
            inj_active ? "Injured " : "");
        if (flags[0] == '\0') {
            snprintf(flags, sizeof(flags), "Active");
        }

        kbo_window_text_appendf(
            &buffer,
            "%c %-24.24s %-11u %-10u %-7u %s\r\n",
            player_id == g_kbo_hub_selected_foreign_player_id ? '>' : ' ',
            player_name,
            player_id,
            current_team_id,
            nation_id,
            flags);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}
