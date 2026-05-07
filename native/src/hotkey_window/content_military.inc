static void kbo_build_military_service_window_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

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

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80", "SERVICE TEAMS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("\xec\xb6\x94\xec\xa0\x81 \xec\xa4\x91\xec\x9d\xb8 \xeb\xb3\xb5\xeb\xac\xb4 \xeb\xb0\xb0\xec\xa0\x95: %ld\r\n\r\n", "Tracked service assignments: %ld\r\n\r\n"),
        g_active_military_loan_count);

    kbo_window_text_appendf(
        &buffer, "%s: serving=%d, returning soon=%d, ready now=%d\r\n",
        sang_name, sang_count, sang_due_60, sang_due_now);
    kbo_window_text_appendf(
        &buffer, "%s: serving=%d, returning soon=%d, ready now=%d\r\n\r\n",
        kpb_name, kpb_count, kpb_due_60, kpb_due_now);

    uintptr_t player_vector = 0;
    int32_t   player_count  = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "CURRENT SERVICE LIST\r\n");
    kbo_window_text_appendf(&buffer, "PLAYER                   SERVICE TEAM              ORIGINAL CLUB             RETURN DATE  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "------------------------------------------------------------------------------------------------\r\n");

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        const char* service_fallback = NULL;
        uint32_t service_team_id = 0;
        if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
            service_team_id  = sang_id;
            service_fallback = sang_name;
        } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
            service_team_id  = kpb_id;
            service_fallback = kpb_name;
        }

        if (service_team_id == 0) {
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id);
        kbo_military_repair_original_team_memory(
            player,
            original_team_id,
            original_league_id,
            service_team_id,
            sang_id,
            kpb_id);
        int32_t days_left     = kbo_military_effective_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        uint8_t loan_active     = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        const char* status = days_left <= 0 ? "Ready to return" : (days_left <= 60 ? "Returning soon" : "Serving");
        if      (military_active == 0) { status = "Needs review"; }
        else if (loan_active     == 0) { status = "Club-only";    }

        char player_name[64]       = {0};
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

        kbo_window_text_appendf(
            &buffer,
            "%-24.24s %-25.25s %-25.25s %-11.11s  %s\r\n",
            player_name, service_team_name, original_team_name,
            return_date, status);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}
