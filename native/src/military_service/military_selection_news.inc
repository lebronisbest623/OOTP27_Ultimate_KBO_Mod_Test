/* Military service selection news text and link helpers. Included from native/src/military_service_loan.inc. */

static void kbo_military_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_military_copy_team_history_name(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (team != NULL) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        if (full_name[0] != '\0') {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0') {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (nickname[0] != '\0') {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
    }
    snprintf(out, out_size, "%s", fallback != NULL ? fallback : "his original club");
}

static void kbo_military_copy_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (team_id == 0u) {
        return;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    char team_name[96] = {0};
    kbo_military_copy_team_history_name(team, team_name, sizeof(team_name), NULL);
    if (team_name[0] == '\0' || ascii_equals_ignore_case(team_name, "his original club")) {
        snprintf(team_name, sizeof(team_name), "Team #%u", team_id);
    }
    snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
}

static void kbo_military_copy_player_link(uint32_t player_id, uintptr_t player_ptr, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (player_id == 0u) {
        return;
    }

    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_military_find_player_by_id(player_id);
    }

    char player_name[64] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", player_id);
    }

    snprintf(out, out_size, "<%s:player#%u>", player_name, player_id);
}

static int kbo_emit_military_selection_news(
    uint32_t event_yyyymmdd,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source)
{
    if (event_yyyymmdd == 0u || entries == NULL || entry_count <= 0) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        append_logf(
            "KBO military selection news skipped source=%s date=%u reason=league_id_unavailable count=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            entry_count);
        return 0;
    }

    char title[160] = {0};
    snprintf(
        title,
        sizeof(title),
        "Sangmu Selects %d Player%s for Military Service",
        entry_count,
        entry_count == 1 ? "" : "s");

    char body[8192] = {0};
    int len = snprintf(
        body,
        sizeof(body),
        "Sangmu completed its annual military-service selection, adding %d player%s from the offseason military-service pool. The group will report to the service club while their original KBO clubs retain their rights until the scheduled return date.\n\nSelected players:\n",
        entry_count,
        entry_count == 1 ? "" : "s");
    if (len < 0) {
        body[0] = '\0';
    }
    size_t used = strlen(body);

    for (int i = 0; i < entry_count && used + 180u < sizeof(body); i++) {
        char player_link[96] = {0};
        char team_link[128] = {0};
        kbo_military_copy_player_link(entries[i].player_id, entries[i].player_ptr, player_link, sizeof(player_link));
        kbo_military_copy_team_link(entries[i].original_team_id, team_link, sizeof(team_link));
        len = snprintf(
            body + used,
            sizeof(body) - used,
            "%d. %s%s%s\n",
            i + 1,
            player_link[0] != '\0' ? player_link : "Selected player",
            team_link[0] != '\0' ? ", " : "",
            team_link);
        if (len <= 0) {
            break;
        }
        used += (size_t)len;
    }

    if (used + 220u < sizeof(body)) {
        snprintf(
            body + used,
            sizeof(body) - used,
            "\nThe selections were made from players newly marked for military service after the season, with player value and service-club roster space considered before the final list was posted.");
    }

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO military selection news source=%s date=%u league_id=%u count=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        league_id,
        entry_count,
        created);
    return created;
}

