static void kbo_build_foreign_policy_hub_text(char* out, size_t out_size)
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
    kbo_window_text_appendf(&buffer, "KBO FOREIGN PLAYERS\r\n");
    kbo_window_text_appendf(
        &buffer,
        "Policy: %s / Base effective limit: %u / Asian-quota nation IDs: %d\r\n\r\n",
        kbo_custom_foreign_policy_enabled() ? "Custom KBO layer" : "OOTP fallback",
        KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        configured_nations);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  SLOT  PLAYER                   ID          TEAM       ACTIVE     NAT     STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------\r\n");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
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
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
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
            "  %-5.5s %-24.24s %-11u %-10u %-10u %-7u %s\r\n",
            kbo_hub_foreign_slot_code_for_player(player),
            player_name,
            player_id,
            current_team_id,
            active_team_id,
            nation_id,
            flags);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }

}
