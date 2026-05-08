static int run_kbo_allstar_native_event_generation(uintptr_t league_ptr, const char* source)
{
    if (InterlockedCompareExchange(&g_allstar_native_event_generation_done, 0, 0) != 0) {
        return 1;
    }
    if (InterlockedCompareExchange(&g_allstar_native_event_generation_in_progress, 1, 0) != 0) {
        return 0;
    }

    int generated = 0;
    do {
        if (!kbo_allstar_league_context_enabled(league_ptr)) {
            break;
        }

        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint8_t* league = (uint8_t*)league_ptr;
        if (!memory_range_readable(league, layout.league_id_fallback_offset + sizeof(uint32_t))) {
            append_logf("KBO all-star native events skipped source=%s league=%p reason=league_unreadable", source != NULL ? source : "", league);
            break;
        }

        uintptr_t imported_league_ptr = (uintptr_t)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            NULL,
            NULL);
        if (imported_league_ptr != league_ptr) {
            append_logf(
                "KBO all-star native events skipped source=%s league=%p imported=%p reason=not_schedule_import_league",
                source != NULL ? source : "",
                league,
                (void*)imported_league_ptr);
            break;
        }

        enable_kbo_allstar_flags(league_ptr, source != NULL ? source : "native_allstar_events");
        if ((!kbo_allstar_season_start_date_ready(league) || !kbo_allstar_schedule_date_ready(league))
                && !seed_kbo_allstar_schedule_dates(league_ptr, source)) {
            log_kbo_allstar_native_event_state("waiting_date", league_ptr, source);
            break;
        }
        if (!kbo_allstar_season_start_date_ready(league) || !kbo_allstar_schedule_date_ready(league)) {
            log_kbo_allstar_native_event_state("waiting_date", league_ptr, source);
            break;
        }

        OotpMakeAllstarGameEventsFn make_events =
            (OotpMakeAllstarGameEventsFn)InterlockedCompareExchangePointer(
                (PVOID volatile*)&g_allstar_make_events_ptr,
                NULL,
                NULL);
        if (make_events == NULL) {
            const uint8_t may_expected[19] = {
                0x41, 0x80, 0xBC, 0x24, 0xF0, 0x45, 0x00, 0x00, 0x00,
                0x74, 0x08,
                0x49, 0x8B, 0xCC,
                0xE8, 0xB9, 0x55, 0xFF, 0xFF
            };
            const uint8_t april_expected[19] = {
                0x41, 0x80, 0xBC, 0x24, 0xE8, 0x45, 0x00, 0x00, 0x00,
                0x74, 0x08,
                0x49, 0x8B, 0xCC,
                0xE8, 0x89, 0x55, 0xFF, 0xFF
            };
            uint8_t* event_site = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
            if (event_site == NULL) {
                event_site = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
            }
            if (event_site != NULL) {
                make_events = (OotpMakeAllstarGameEventsFn)resolve_relative_call_target(
                    event_site + OOTP27_MAKE_ALLSTAR_EVENTS_PREP_CALL_OFFSET);
                if (make_events != NULL) {
                    InterlockedExchangePointer(
                        (PVOID volatile*)&g_allstar_make_events_ptr,
                        (PVOID)make_events);
                }
            }
        }
        if (make_events == NULL) {
            append_logf("KBO all-star native events skipped source=%s reason=target_unresolved", source != NULL ? source : "");
            break;
        }
        if (!memory_range_readable((void*)make_events, 32u)) {
            append_logf("KBO all-star native events skipped source=%s target=%p reason=target_unreadable", source != NULL ? source : "", (void*)make_events);
            break;
        }
        if (!kbo_allstar_league_vtable_plausible(league_ptr)) {
            append_logf("KBO all-star native events skipped source=%s league=%p reason=league_vtable_invalid", source != NULL ? source : "", league);
            break;
        }

        log_kbo_allstar_native_event_state("before_call", league_ptr, source);
        make_events(league_ptr, 1);
        log_kbo_allstar_native_event_state("after_call", league_ptr, source);

        InterlockedExchange(&g_allstar_native_event_generation_done, 1);
        generated = 1;
    } while (0);

    InterlockedExchange(&g_allstar_native_event_generation_in_progress, 0);
    return generated;
}
