#include <stdio.h>
#include <string.h>
/* Core league event creation helpers. */

static int kbo_league_event_exists(
    uintptr_t event_manager,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title)
{
    if (event_manager == 0 || title == NULL || title[0] == '\0') {
        return 0;
    }
    if (!memory_range_readable((void*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET), sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)event_type) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)year) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)month
                || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)day) {
            continue;
        }

        char existing_title[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, existing_title, sizeof(existing_title))) {
            continue;
        }
        if (ascii_equals_ignore_case(existing_title, title)) {
            return 1;
        }
    }

    return 0;
}

static int create_kbo_league_event(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title,
    uint16_t aux_id,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0 || year < 1800 || year > 2200
            || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("league event create skipped source=%s title=%s reason=no_exe_module", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0) {
        append_logf(
            "league event create skipped source=%s title=%s reason=event_manager_unreadable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    if (kbo_league_event_exists(event_manager, year, month, day, league_id, event_type, title)) {
        append_logf(
            "league event create skipped source=%s title=%s reason=already_exists date=%04u-%02u-%02u league_id=%u type=%u manager=%p",
            source != NULL ? source : "",
            title,
            year,
            month,
            day,
            league_id,
            event_type,
            (void*)event_manager);
        return 0;
    }

    OotpCreateLeagueEventFn create_event =
        (OotpCreateLeagueEventFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_CREATE_LEAGUE_EVENT_RVA);
    if (!memory_range_readable((void*)create_event, 16)) {
        append_logf("league event create skipped source=%s title=%s reason=build_specific_create_func_unavailable fn=%p", source != NULL ? source : "", title, (void*)create_event);
        return 0;
    }

    uint8_t ootp_date[16] = {0};
    *(uint16_t*)(ootp_date + 8) = (uint16_t)year;
    ootp_date[10] = (uint8_t)day;
    ootp_date[11] = (uint8_t)month;

    void* event = create_event(
        (void*)event_manager,
        ootp_date,
        event_type,
        league_id,
        title,
        aux_id);

    append_logf(
        "league event create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u aux=%u manager=%p event=%p",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        event_type,
        (uint32_t)aux_id,
        (void*)event_manager,
        event);
    append_logf(
        "league event verify source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u present=%d",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        event_type,
        kbo_league_event_exists(event_manager, year, month, day, league_id, event_type, title));
    return event != NULL;
}
