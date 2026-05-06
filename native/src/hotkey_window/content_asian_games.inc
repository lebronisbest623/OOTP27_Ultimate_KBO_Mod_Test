static void kbo_build_asian_games_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_clear_asian_games_roster_if_save_changed("hotkey_text");

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0) {
        kbo_load_asian_games_roster_csv("hotkey_text");
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

    kbo_window_text_appendf(&buffer, "ASIAN GAMES ROSTER\r\n");
    kbo_window_text_appendf(
        &buffer,
        "Year: %u / Selected: %ld / Departed: %d / Returned: %d / Exempted: %d\r\n\r\n",
        g_kbo_asian_games_roster_year,
        roster_count,
        departed,
        returned,
        exempted);

    if (roster_count == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
        kbo_window_text_appendf(&buffer, "Advance through the roster-selection event, then refresh this panel.\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "  #  PLAYER                   ID          TEAM       LG         AGE ROLE WC  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------------\r\n");

    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        uintptr_t player_ptr = entry->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
        }

        char player_name[64] = {0};
        if (kbo_player_pointer_plausible(player_ptr)) {
            kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
        } else {
            snprintf(player_name, sizeof(player_name), "Unknown player");
        }

        const char* status = "Selected";
        if (entry->returned) {
            status = entry->exempted ? "Returned/Exempt" : "Returned";
        } else if (entry->departed) {
            status = "Departed/IL";
        }

        kbo_window_text_appendf(
            &buffer,
            "  %02ld %-24.24s %-11u %-10u %-10u %-3u %-4s %-3s %s\r\n",
            i + 1,
            player_name,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->age,
            kbo_asian_games_role_bucket_label(entry->role),
            entry->wildcard ? "YES" : "NO",
            status);
    }
}
