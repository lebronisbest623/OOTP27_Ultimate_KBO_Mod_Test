static int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size);
static int kbo_resolve_foreign_waiver_top_candidate_for_team(uint32_t team_id, uint32_t* out_player_id, uint32_t* out_current_team_id);
static int kbo_hub_count_service_players(uint32_t service_team_id, int* out_due_60, int* out_due_now)
{
    if (out_due_60 != NULL) { *out_due_60 = 0; }
    if (out_due_now != NULL) { *out_due_now = 0; }
    if (service_team_id == 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    int due_60 = 0;
    int due_now = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            continue;
        }
        int32_t days_left = kbo_military_days_left(player);
        if (days_left <= 0) { due_now++; }
        if (days_left > 0 && days_left <= 60) { due_60++; }
        count++;
    }

    if (out_due_60 != NULL)  { *out_due_60  = due_60;  }
    if (out_due_now != NULL) { *out_due_now = due_now; }
    return count;
}

static void kbo_build_overview_hub_text(char* out, size_t out_size)
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
    int sang_count = kbo_hub_count_service_players(sang_id, NULL, NULL);
    int kpb_count  = kbo_hub_count_service_players(kpb_id,  NULL, NULL);

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xec\x9a\x94\xec\x95\xbd", "OVERVIEW"));

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xea\xb5\xb0\xeb\xb3\xb5\xeb\xac\xb4", "MILITARY SERVICE"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xec\x83\x81\xeb\xac\xb4 \xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91: %d\xeb\xaa\x85\r\n", "  Sangmu serving: %d\r\n"),
        sang_count);
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xea\xb2\xbd\xec\xb0\xb0 \xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91: %d\xeb\xaa\x85\r\n", "  Police serving: %d\r\n"),
        kpb_count);
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xec\xb6\x94\xec\xa0\x81 \xec\xa4\x91\xec\x9d\xb8 \xeb\xb3\xb5\xeb\xac\xb4 \xeb\xb0\xb0\xec\xa0\x95: %ld\xea\xb1\xb4\r\n\r\n", "  Tracked assignments: %ld\r\n\r\n"),
        g_active_military_loan_count);

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c", "PATCH STATUS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  KBO Fix: %s\r\n", "  KBO Fix: %s\r\n"),
        kbo_fix_enabled() ? kbo_hub_text("\xec\xbc\x9c\xec\xa7\x90", "enabled") : kbo_hub_text("\xea\xba\xbc\xec\xa7\x90", "disabled"));
    char foreign_waiver_status[220] = {0};
    if (kbo_get_foreign_waiver_window_status_text(foreign_waiver_status, sizeof(foreign_waiver_status))) {
        kbo_window_text_appendf(&buffer, "%s\r\n", foreign_waiver_status);
    }
}

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

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xea\xb5\xb0\xeb\xb3\xb5\xeb\xac\xb4 \xed\x8c\x80", "SERVICE TEAMS"));
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
        kbo_window_text_appendf(&buffer, "Player vector was not available.\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "CURRENT SERVICE LIST\r\n");
    kbo_window_text_appendf(&buffer, "PLAYER                   SERVICE TEAM              ORIGINAL CLUB             DAYS LEFT  STATUS\r\n");
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

        uint32_t original_team_id = *(uint32_t*)(player + 0x5cu);
        if (g_kbo_hub_selected_team_id != 0 && original_team_id != g_kbo_hub_selected_team_id) {
            continue;
        }

        int32_t days_left     = kbo_military_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        uint8_t loan_active     = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        const char* status = days_left <= 0 ? "Ready to return" : (days_left <= 60 ? "Returning soon" : "Serving");
        if      (military_active == 0) { status = "Needs review"; }
        else if (loan_active     == 0) { status = "Club-only";    }

        char player_name[64]       = {0};
        char service_team_name[64] = {0};
        char original_team_name[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        kbo_hub_copy_team_display_name_by_id(service_team_id,  service_team_name,  sizeof(service_team_name),  service_fallback);
        kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);

        kbo_window_text_appendf(
            &buffer,
            "%-24.24s %-25.25s %-25.25s %9d  %s\r\n",
            player_name, service_team_name, original_team_name,
            days_left, status);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "No Sangmu / Police assignments found in the current save.\r\n");
    }
}

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
        kbo_window_text_appendf(&buffer, "Player vector was not available.\r\n");
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
        kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xed\x98\x84\xec\x9e\xac \xec\x84\xa0\xed\x83\x9d\xed\x95\x9c \xed\x8c\x80\xec\x9d\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c \xed\x9b\x84\xeb\xb3\xb4\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.", "No reserve-rights candidates found for the selected team."));
    }
}

static void kbo_build_mod_info_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4", "LICENSE"));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "  \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xec\x9d\x98 LICENSE, README, CREDITS \xeb\xac\xb8\xec\x84\x9c\xeb\xa5\xbc \xed\x99\x95\xec\x9d\xb8\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.",
            "  See LICENSE, README, and CREDITS in the repository."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "  \xec\x9d\xb4 \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 OOTP Developments\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
            "  This mod is not an official OOTP Developments product."));
}

static void kbo_build_asian_quota_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    int configured_nations = kbo_load_asian_quota_nation_ids_once();
    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);
    kbo_window_text_appendf(&buffer, "ASIAN QUOTA CANDIDATES\r\n");
    kbo_window_text_appendf(&buffer, "Configured nation IDs: %d\r\n\r\n", configured_nations);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  PLAYER                   ID          TEAM       ACTIVE     NAT     STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------\r\n");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "Player vector was not available.\r\n");
        return;
    }

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (player_id == 0u
                || current_team_id != g_kbo_hub_selected_team_id
                || !kbo_player_is_asian_quota_candidate(player)) {
            continue;
        }

        char player_name[64] = {0};
        char flags[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        snprintf(flags, sizeof(flags), "%s%s%s%s%s",
            player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
            player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
            player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
            player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
            player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
        if (flags[0] == '\0') {
            snprintf(flags, sizeof(flags), "Active");
        }

        kbo_window_text_appendf(
            &buffer,
            "  %-24.24s %-11u %-10u %-10u %-7u %s\r\n",
            player_name,
            player_id,
            current_team_id,
            active_team_id,
            nation_id,
            flags);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "No Asian-quota candidates found for the selected team.\r\n");
    }
}

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
        kbo_window_text_appendf(&buffer, "No Asian Games roster CSV is available yet.\r\n");
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
            snprintf(player_name, sizeof(player_name), "Player #%u", entry->player_id);
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

static void kbo_build_settings_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "SETTINGS"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x96\xb8\xec\x96\xb4", "LANGUAGE"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xed\x98\x84\xec\x9e\xac \xec\x96\xb8\xec\x96\xb4: %s\r\n", "  Current language: %s\r\n"),
        g_kbo_hub_language == KBO_HUB_LANG_EN ? "English" : "\xED\x95\x9c\xEA\xB5\xAD\xEC\x96\xB4");
    kbo_window_text_appendf(&buffer, "\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\x8b\xa8\xec\xb6\x95\xed\x82\xa4", "HOTKEYS"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("  F2  \xec\xb0\xbd \xec\x97\xb4\xea\xb8\xb0 / \xeb\x8b\xab\xea\xb8\xb0", "  F2  Open / close window"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("  F5  \xed\x98\x84\xec\x9e\xac \xed\x8c\xa8\xeb\x84\x90 \xec\x83\x88\xeb\xa1\x9c\xea\xb3\xa0\xec\xb9\xa8", "  F5  Refresh current panel"));
}

static void kbo_build_hub_window_text(char* out, size_t out_size)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: kbo_build_military_service_window_text(out, out_size); return;
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: kbo_build_foreign_rights_window_text(out, out_size); return;
    case KBO_HUB_VIEW_ASIAN_QUOTA: kbo_build_asian_quota_hub_text(out, out_size);       return;
    case KBO_HUB_VIEW_ASIAN_GAMES: kbo_build_asian_games_hub_text(out, out_size);       return;
    case KBO_HUB_VIEW_MOD_INFO: kbo_build_mod_info_hub_text(out, out_size);            return;
    case KBO_HUB_VIEW_SETTINGS: kbo_build_settings_hub_text(out, out_size);            return;
    case KBO_HUB_VIEW_OVERVIEW:
    default:                    kbo_build_overview_hub_text(out, out_size);             return;
    }
}


static void kbo_refresh_hotkey_window(void)
{
    if (g_kbo_hotkey_edit == NULL) {
        return;
    }

    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 65536u);
    if (text == NULL) {
        SetWindowTextA(g_kbo_hotkey_edit, "KBO Companion\r\n\r\nCould not prepare this page.");
        return;
    }

    kbo_build_hub_window_text(text, 65536u);
    HFONT content_font = (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES)
        ? g_kbo_hub_font_mono : g_kbo_hub_font_body;
    if (content_font != NULL) {
        SendMessageA(g_kbo_hotkey_edit, WM_SETFONT, (WPARAM)content_font, TRUE);
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                SetWindowTextW(g_kbo_hotkey_edit, wide);
            } else {
                SetWindowTextA(g_kbo_hotkey_edit, text);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        } else {
            SetWindowTextA(g_kbo_hotkey_edit, text);
        }
    } else {
        SetWindowTextA(g_kbo_hotkey_edit, text);
    }
    HeapFree(GetProcessHeap(), 0, text);
}




