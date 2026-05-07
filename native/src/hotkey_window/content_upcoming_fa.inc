static void kbo_build_upcoming_fa_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    uint32_t current_year = 0u;
    kbo_current_year_relaxed(&current_year);

    char team_name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(g_kbo_hub_selected_team_id, team_name, sizeof(team_name), NULL);

    kbo_window_text_appendf(&buffer, "UPCOMING FA\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", team_name[0] != '\0' ? team_name : "Selected club");
    kbo_window_text_appendf(&buffer, "PLAYER                   CLUB                      LAST FA  ELIGIBLE  LEFT  COUNT  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "------------------------------------------------------------------------------------------\r\n");

    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int rendered = 0;
    uint32_t max_year = current_year != 0u ? current_year + 3u : 2200u;

    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count && rendered < 500; i++) {
            KboFaRequalificationRecord* rec = &records[i];
            uint32_t eligible_year = rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS;
            if (rec->original_team_id != g_kbo_hub_selected_team_id) {
                continue;
            }
            if (current_year != 0u) {
                if (pass == 0 && (eligible_year < current_year || eligible_year > max_year)) {
                    continue;
                }
                if (pass == 1 && eligible_year >= current_year) {
                    continue;
                }
            } else if (pass == 1) {
                continue;
            }

            uint8_t* player = kbo_find_fa_requalification_player_by_id(rec->player_id);
            char player_name[96] = {0};
            if (player != NULL) {
                kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            }
            if (player_name[0] == '\0') {
                snprintf(player_name, sizeof(player_name), "Player #%u", rec->player_id);
            }

            const char* status = "Scheduled";
            int32_t years_left = 0;
            if (current_year != 0u) {
                years_left = (int32_t)eligible_year - (int32_t)current_year;
                if (years_left <= 0) {
                    status = "Eligible";
                } else if (years_left == 1) {
                    status = "Next year";
                } else {
                    status = "Upcoming";
                }
            }

            kbo_window_text_appendf(
                &buffer,
                "%-24.24s %-25.25s %7u  %8u  %4d  %5u  %s\r\n",
                player_name,
                team_name[0] != '\0' ? team_name : "-",
                rec->last_fa_year,
                eligible_year,
                years_left,
                rec->fa_count,
                status);
            rendered++;
        }
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "No FA requalification records for the selected club.\r\n");
    }
}
