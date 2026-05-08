static void kbo_emit_fa_compensation_player_selected_news(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t decided_yyyymmdd)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || decided_yyyymmdd == 0u) {
        return;
    }

    uint32_t year = decided_yyyymmdd / 10000u;
    uint32_t month = (decided_yyyymmdd / 100u) % 100u;
    uint32_t day = decided_yyyymmdd % 100u;
    if (year < 1982u || month == 0u || day == 0u) {
        return;
    }

    char cash_text[32] = "-";
    kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_text, sizeof(cash_text));

    char selected_name[96] = {0};
    snprintf(
        selected_name,
        sizeof(selected_name),
        "%s",
        selected->player_name[0] != '\0' ? selected->player_name : "Compensation player");

    char history_text[512] = {0};
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Selected as KBO FA compensation player for %s; rights transfer from Team #%u to Team #%u is recorded with %s cash compensation.",
        rec->player_name[0] != '\0' ? rec->player_name : "FA signing",
        rec->signing_team_id,
        rec->original_team_id,
        cash_text);
    insert_kbo_player_history_sql(selected->player_id, year, month, day, history_text, "fa_compensation_player");

    char title[160] = {0};
    snprintf(title, sizeof(title), "KBO FA compensation player: %s", selected_name);

    char body[1200] = {0};
    snprintf(
        body,
        sizeof(body),
        "<%s:player#%u> has been selected by <Team #%u:team#%u> as the Grade %s KBO FA compensation player after <%s:player#%u> signed with <Team #%u:team#%u>.\n\nThe compensation package recorded by the KBO mod is %s plus one eligible player from outside <Team #%u:team#%u>'s protected list.",
        selected_name,
        selected->player_id,
        rec->original_team_id,
        rec->original_team_id,
        rec->grade,
        rec->player_name[0] != '\0' ? rec->player_name : "FA player",
        rec->player_id,
        rec->signing_team_id,
        rec->signing_team_id,
        cash_text,
        rec->signing_team_id,
        rec->signing_team_id);

    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}

static int kbo_transfer_fa_compensation_player_to_original_team(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t transfer_yyyymmdd,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || rec->original_team_id == 0u) {
        return 0;
    }

    uint32_t before_current_team = 0u;
    uint32_t before_current_league = 0u;
    uint8_t* player = kbo_find_player_by_id(selected->player_id, &before_current_team, &before_current_league);
    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL || destination_team == NULL) {
        append_logf(
            "KBO FA compensation transfer failed reason=lookup fa_player=%u selected=%u player=%p team=%p original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            player,
            destination_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    uint32_t before_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (before_current_team != rec->signing_team_id && before_active_team != rec->signing_team_id) {
        append_logf(
            "KBO FA compensation transfer skipped reason=player_not_on_signing_team fa_player=%u selected=%u current=%u active=%u signing_team=%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            before_active_team,
            rec->signing_team_id,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_like_ootp(
        player,
        destination_team,
        rec->league_id,
        &called_pre_change,
        &called_register,
        &called_attach);

    uint32_t after_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (after_current_team != rec->original_team_id && after_active_team != rec->original_team_id) {
        append_logf(
            "KBO FA compensation transfer failed reason=post_verify fa_player=%u selected=%u current=%u->%u active=%u->%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            after_current_team,
            before_active_team,
            after_active_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int year = (int)(transfer_yyyymmdd / 10000u);
    int month = (int)((transfer_yyyymmdd / 100u) % 100u);
    int day = (int)(transfer_yyyymmdd % 100u);
    char history_text[256] = {0};
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Transferred to Team #%u as the KBO FA compensation player for %s.",
        rec->original_team_id,
        rec->player_name[0] != '\0' ? rec->player_name : "the signing");
    insert_kbo_player_history_sql(selected->player_id, year, month, day, history_text, "fa_compensation_transfer");

    append_logf(
        "KBO FA compensation player transferred fa_player=%u selected=%u name=%s signing_team=%u original_team=%u current=%u->%u active=%u->%u pre=%d register=%d attach=%d source=%s",
        rec->player_id,
        selected->player_id,
        selected->player_name,
        rec->signing_team_id,
        rec->original_team_id,
        before_current_team,
        after_current_team,
        before_active_team,
        after_active_team,
        called_pre_change,
        called_register,
        called_attach,
        source != NULL ? source : "");
    return 1;
}
