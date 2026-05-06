static void kbo_build_foreign_injury_replacement_hub_text(char* out, size_t out_size)
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
    kbo_foreign_injury_replacement_scan_once("hotkey_text");

    int open_count = 0;
    int pending_count = 0;
    int closed_count = 0;
    kbo_count_foreign_injury_replacements_for_team(selected_team_id, &open_count, &pending_count, &closed_count);

    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);

    kbo_window_text_appendf(&buffer, "FOREIGN INJURY REPLACEMENT\r\n");
    kbo_window_text_appendf(&buffer, "Team: %s (%u)\r\n", selected_team_name, selected_team_id);
    kbo_window_text_appendf(&buffer, "Open: %d / Decision due: %d / Closed: %d\r\n", open_count, pending_count, closed_count);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  SLOT          INJURED PLAYER            ID          REPLACEMENT              ID          DAYS  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "--------------------------------------------------------------------------------------------------------\r\n");

    int rendered = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count && rendered < 500; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (selected_team_id != 0u && rec->team_id != selected_team_id) {
            continue;
        }

        char player_name[64] = {0};
        char replacement_name[64] = {0};
        int days_left = 0;
        uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            days_left = (int)*(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        } else {
            snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
        }
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        if (replacement != NULL && memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_hub_copy_player_display_name(replacement, replacement_name, sizeof(replacement_name));
        } else if (rec->replacement_player_id != 0u) {
            snprintf(replacement_name, sizeof(replacement_name), "Player #%u", rec->replacement_player_id);
        } else {
            snprintf(replacement_name, sizeof(replacement_name), "-");
        }

        kbo_window_text_appendf(
            &buffer,
            "  %-13.13s %-24.24s %-11u %-24.24s %-11u %-5d %s\r\n",
            kbo_foreign_injury_slot_label(rec->slot_type),
            player_name,
            rec->injured_player_id,
            replacement_name,
            rec->replacement_player_id,
            days_left,
            kbo_foreign_injury_status_label(rec->status));
        rendered++;
    }
    kbo_unlock_foreign_injury_replacements();

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}
