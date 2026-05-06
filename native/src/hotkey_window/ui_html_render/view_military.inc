static void kbo_webview_append_military_view(KboWindowTextBuffer* buffer)
{
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
    
            kbo_window_text_appendf(buffer, "<div class='rights'><div class='reportbar'><div><b>KBO MILITARY SERVICE</b><div class='muted'>Tracked assignments: %ld. ",
                g_active_military_loan_count);
            kbo_html_append_escaped(buffer, sang_name);
            kbo_window_text_appendf(buffer, ": %d serving / %d due soon / %d ready. ", sang_count, sang_due_60, sang_due_now);
            kbo_html_append_escaped(buffer, kpb_name);
            kbo_window_text_appendf(buffer, ": %d serving / %d due soon / %d ready.</div></div><div class='help'><span>Original club</span><span>F5 refresh</span></div></div>",
                kpb_count, kpb_due_60, kpb_due_now);
            kbo_window_text_appendf(buffer, "<section class='tablewrap'><table><thead><tr><th></th><th>PLAYER</th><th>SERVICE TEAM</th><th>ORIGINAL CLUB</th><th>RETURN DATE</th></tr></thead><tbody>");
    
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
    
                    kbo_window_text_appendf(buffer, "<tr><td class='sel'>%s</td>", service_code);
                    kbo_webview_append_player_name_cell(buffer, player_name, player_id);
                    kbo_window_text_appendf(buffer, "<td>");
                    kbo_html_append_escaped(buffer, service_team_name);
                    kbo_window_text_appendf(buffer, "</td><td>");
                    kbo_html_append_escaped(buffer, original_team_name);
                    kbo_window_text_appendf(buffer, "</td><td>%s</td></tr>", return_date);
                    rendered++;
                }
                if (rendered == 0) {
                    kbo_window_text_appendf(buffer, "<tr><td colspan='5'></td></tr>");
                }
            } else {
                kbo_window_text_appendf(buffer, "<tr><td colspan='5'></td></tr>");
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
