/* Core native news object construction helpers. */

static int assign_kbo_news_pointer_string(void* news_object, uint32_t offset, const char* text)
{
    if (news_object == NULL || text == NULL
            || !memory_range_readable((uint8_t*)news_object + offset, sizeof(void*))) {
        return 0;
    }

    void* string_object = *(void**)((uint8_t*)news_object + offset);
    if (string_object == NULL || !memory_range_readable(string_object, 0x18)) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    OotpCoreStringAssignFn assign_string = (OotpCoreStringAssignFn)((uintptr_t)exe + OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable((void*)assign_string, 16)) {
        return 0;
    }

    assign_string(string_object, text);
    return 1;
}

static uint32_t kbo_read_news_u32(void* object, uint32_t offset)
{
    if (object == NULL || !memory_range_readable((uint8_t*)object + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)((uint8_t*)object + offset);
}

static int create_kbo_real_add_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body,
    const char* source)
{
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t global = get_ootp_global_database();
    uintptr_t manager = global;
    if (exe == NULL || manager == 0 || title == NULL || title[0] == '\0') {
        append_logf(
            "league news real_add skipped source=%s title=%s reason=no_exe_manager_or_title exe=%p manager=%p global=%p",
            source != NULL ? source : "",
            title != NULL ? title : "",
            exe,
            (void*)manager,
            (void*)global);
        return 0;
    }

    OotpOperatorNewFn ootp_new = (OotpOperatorNewFn)((uintptr_t)exe + OOTP27_UI_OPERATOR_NEW_RVA);
    OotpNewsObjectCtorFn ctor = (OotpNewsObjectCtorFn)((uintptr_t)exe + OOTP27_NEWS_OBJECT_CTOR_RVA);
    OotpNewsStringEnsureFn ensure_strings = (OotpNewsStringEnsureFn)((uintptr_t)exe + OOTP27_NEWS_STRING_ENSURE_RVA);
    OotpLeagueNewsRealAddFn real_add = (OotpLeagueNewsRealAddFn)((uintptr_t)exe + OOTP27_LEAGUE_NEWS_REAL_ADD_RVA);
    if (!memory_range_readable((void*)ootp_new, 16)
            || !memory_range_readable((void*)ctor, 16)
            || !memory_range_readable((void*)ensure_strings, 16)
            || !memory_range_readable((void*)real_add, 16)) {
        append_logf(
            "league news real_add skipped source=%s title=%s reason=func_unreadable new=%p ctor=%p ensure=%p add=%p",
            source != NULL ? source : "",
            title,
            (void*)ootp_new,
            (void*)ctor,
            (void*)ensure_strings,
            (void*)real_add);
        return 0;
    }

    uint8_t* news = (uint8_t*)ootp_new(OOTP27_MESSAGE_OBJECT_SIZE);
    if (news == NULL || !memory_range_readable(news, OOTP27_MESSAGE_OBJECT_SIZE)) {
        append_logf(
            "league news real_add skipped source=%s title=%s reason=alloc_failed object=%p",
            source != NULL ? source : "",
            title,
            news);
        return 0;
    }

    ctor(news);
    ensure_strings(news);

    *(uint32_t*)(news + 0x44u) = league_id;
    *(uint32_t*)(news + 0x50u) = 3u;
    *(uint32_t*)(news + 0x54u) = 0u;
    *(uint32_t*)(news + 0x58u) = 1u;
    *(uint32_t*)(news + 0x5cu) = 3u;
    *(uint32_t*)(news + 0x60u) = league_id;
    *(uint32_t*)(news + 0x64u) = 0xffffffffu;
    *(uint32_t*)(news + 0x68u) = 0u;
    *(uint16_t*)(news + 0x78u) = (uint16_t)year;
    *(news + 0x7au) = (uint8_t)day;
    *(news + 0x7bu) = (uint8_t)month;
    *(uint32_t*)(news + 0x7cu) = 0u;
    *(uint32_t*)(news + 0x80u) = 0x00000100u;
    *(uint32_t*)(news + 0x84u) = 0xffffffffu;
    *(uint32_t*)(news + 0x88u) = 0x00000001u;
    *(uint32_t*)(news + OOTP27_MESSAGE_OBJECT_ID_OFFSET) = 0u;

    int title_ok = assign_kbo_news_pointer_string(news, 0x90u, title);
    int body_ok = assign_kbo_news_pointer_string(news, 0x98u, body != NULL ? body : "");
    uint32_t result = real_add((void*)manager, news, 0);
    uint32_t message_id = kbo_read_news_u32(news, OOTP27_MESSAGE_OBJECT_ID_OFFSET);
    if (message_id == 0 && result != 0) {
        message_id = result;
    }
    int body_file_ok = 0;
    if (result != 0 && message_id != 0) {
        body_file_ok = write_kbo_message_body_file(message_id, title, body, source);
    }

    append_logf(
        "league news real_add create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u manager=%p object=%p result=%u id8c=%u title_ok=%d body_ok=%d body_file=%d",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        message_type,
        (void*)manager,
        news,
        result,
        message_id,
        title_ok,
        body_ok,
        body_file_ok);
    return result != 0;
}
