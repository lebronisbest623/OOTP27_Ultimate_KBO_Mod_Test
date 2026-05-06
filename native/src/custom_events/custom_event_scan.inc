/* Custom-event due scan and dispatch. Included from native/src/custom_events.inc. */

static int scan_kbo_custom_events_once(const char* source)
{
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0 || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return -1;
    }

    uint32_t current_year = 0;
    uint32_t current_month = 0;
    uint32_t current_day = 0;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        return -1;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return -1;
    }

    int triggered = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
        if (configured_league_id == 0u) {
            configured_league_id = kbo_resolve_kbo_league_id();
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != configured_league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }

        char name[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (!kbo_custom_event_name_matches_local(name)) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_over = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
        int due = 0;
        if (event_year < current_year) {
            due = 1;
        } else if (event_year == current_year) {
            if (event_month < current_month) {
                due = 1;
            } else if (event_month == current_month && event_day <= current_day) {
                due = 1;
            }
        }

        uint32_t event_yyyymmdd = event_year * 10000u + event_month * 100u + event_day;
        if (!due || event_over != 0 || kbo_custom_event_already_processed(event_ptr)) {
            continue;
        }
        if (kbo_custom_event_processed_marker_exists(event_yyyymmdd, name)) {
            kbo_mark_custom_event_processed(event_ptr);
            append_logf(
                "KBO custom event skipped processed marker source=%s name=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                name,
                event_year,
                event_month,
                event_day);
            continue;
        }

        int action_result = kbo_dispatch_custom_event(
            event_ptr,
            name,
            event_yyyymmdd,
            event_year,
            event_month,
            event_day,
            source);
        if (action_result < 0) {
            continue;
        }

        if (action_result) {
            kbo_mark_custom_event_processed(event_ptr);
            kbo_persist_custom_event_processed_marker(event_yyyymmdd, name, source);
            triggered++;
        }

        append_logf(
            "KBO custom event triggered source=%s name=%s event_date=%04u-%02u-%02u current=%04u-%02u-%02u over=%u event=%p action_result=%d",
            source != NULL ? source : "",
            name,
            event_year,
            event_month,
            event_day,
            current_year,
            current_month,
            current_day,
            event_over,
            (void*)event_ptr,
            action_result);

        if (triggered > 0) {
            break;
        }
    }

    if (triggered > 0) {
        append_logf(
            "KBO custom event scan source=%s current=%04u-%02u-%02u triggered=%d count=%d manager=%p vector=%p",
            source != NULL ? source : "",
            current_year,
            current_month,
            current_day,
            triggered,
            event_count,
            (void*)event_manager,
            (void*)event_vector);
    }
    return triggered;
}


