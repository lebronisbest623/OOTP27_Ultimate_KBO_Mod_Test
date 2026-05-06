/* Foreign injury replacement native news emission. Included from foreign_injury_replacement.inc. */

static void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u || rec->league_id == 0u) {
        return;
    }

    uint32_t event_date = 0u;
    if (!kbo_get_current_yyyymmdd(&event_date) || event_date == 0u) {
        event_date = rec->opened_on_yyyymmdd;
    }
    if (event_date == 0u) {
        return;
    }

    char player_name[96] = {0};
    uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
    if (player != NULL) {
        kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
    }

    char title[128] = {0};
    char body[1024] = {0};
    if (phase != NULL && strcmp(phase, "pending") == 0) {
        snprintf(title, sizeof(title), "[KBO] Foreign Injury Replacement Decision Required");
        snprintf(
            body,
            sizeof(body),
            "The temporary foreign-player injury replacement window for <Team #%u:team#%u> has moved to a decision stage because <%s:player#%u> is no longer listed as unavailable.\n\nThe club must now close the temporary window or convert the replacement into a regular foreign-player slot under the KBO roster limit.",
            rec->team_id,
            rec->team_id,
            player_name,
            rec->injured_player_id);
    } else {
        snprintf(title, sizeof(title), "[KBO] Foreign Injury Replacement Window Opened");
        snprintf(
            body,
            sizeof(body),
            "The KBO approved a temporary foreign-player injury replacement window for <Team #%u:team#%u> after <%s:player#%u> was diagnosed with an injury expected to keep him out for %d days.\n\nThe club may carry one additional %s foreign player while the injured player remains unavailable.",
            rec->team_id,
            rec->team_id,
            player_name,
            rec->injured_player_id,
            days_left > 0 ? days_left : 0,
            kbo_foreign_injury_slot_label(rec->slot_type));
    }

    int created = create_kbo_native_live_news_with_body(
        event_date / 10000u,
        (event_date / 100u) % 100u,
        event_date % 100u,
        rec->league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "foreign injury replacement: news phase=%s team=%u injured=%u league=%u created=%d",
        phase != NULL ? phase : "open",
        rec->team_id,
        rec->injured_player_id,
        rec->league_id,
        created);
}

