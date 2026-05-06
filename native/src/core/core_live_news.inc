/* Core live news fanout helpers. */

static int create_kbo_core_message_news(uint32_t league_id, const char* title, const char* body, const char* source)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t global = get_ootp_global_database();
    if (exe == NULL || global == 0
            || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        append_logf("core message news skipped source=%s title=%s reason=no_global", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    void* live_date = NULL;
    if (current_date != 0 && memory_range_readable((void*)(current_date + 0x1b0u), 16)) {
        live_date = (void*)(current_date + 0x1b0u);
    }
    if (live_date == NULL) {
        append_logf("core message news skipped source=%s title=%s reason=no_live_date", source != NULL ? source : "", title);
        return 0;
    }

    OotpCreateMessageCoreFn create_core = (OotpCreateMessageCoreFn)((uintptr_t)exe + OOTP27_CREATE_MESSAGE_CORE_RVA);
    if (!memory_range_readable((void*)create_core, 16)) {
        append_logf("core message news skipped source=%s title=%s reason=func_unreadable fn=%p", source != NULL ? source : "", title, (void*)create_core);
        return 0;
    }

    char live_text[1024] = {0};
    snprintf(live_text, sizeof(live_text), "%s: %s", title, body != NULL ? body : "");

    uint32_t team_ids[128] = {0};
    int scanned = 0;
    int unreadable = 0;
    int team_count = collect_kbo_league_team_ids(league_id, team_ids, 128, &scanned, &unreadable);
    int created = 0;
    for (int i = 0; i < team_count; i++) {
        create_core((void*)global, 10u, team_ids[i], live_date, live_text);
        created++;
    }
    if (created == 0) {
        create_core((void*)global, 10u, league_id, live_date, live_text);
        created = 1;
    }

    append_logf(
        "core message news fanout source=%s title=%s league_id=%u team_targets=%d scanned=%d unreadable=%d created=%d",
        source != NULL ? source : "",
        title,
        league_id,
        team_count,
        scanned,
        unreadable,
        created);
    return created > 0;
}

static int create_kbo_native_live_news_with_body(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }
    if (body == NULL || body[0] == '\0') {
        body = "";
    }

    int event_created = 0;
    int native_created = 0;
    int league_news_created = insert_kbo_league_news_table_sql(
        year,
        month,
        day,
        league_id,
        title,
        body,
        "native_live_news");
    append_logf(
        "native live news create_news_item skipped title=%s reason=unsafe_rva_call_disabled",
        title);

    int sql_created = insert_kbo_league_news_sql(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        body,
        "native_live_news");
    int real_created = create_kbo_real_add_news(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        body,
        "native_live_news");
    int core_created = 0;
    append_logf(
        "core message news skipped source=%s title=%s reason=unsafe_rva_call_disabled",
        "native_live_news",
        title);
    append_logf(
        "native live news result title=%s date=%04u-%02u-%02u league_id=%u type=%u event=%d league_news=%d sql=%d real=%d native=%d core=%d",
        title,
        year,
        month,
        day,
        league_id,
        message_type,
        event_created,
        league_news_created,
        sql_created,
        real_created,
        native_created,
        core_created);
    if (real_created) {
        return 2;
    }
    return event_created || league_news_created || sql_created || native_created || core_created;
}

static int create_kbo_native_live_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title)
{
    return create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        NULL);
}
