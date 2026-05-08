__declspec(noinline) void ootp_kbo_capture_allstar_schedule_import_league(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !kbo_allstar_league_core_plausible(league_ptr)) {
        return;
    }

    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
    enable_kbo_allstar_flags(league_ptr, "schedule_import");
    seed_kbo_allstar_schedule_dates(league_ptr, "schedule_import");

    static volatile LONG log_count = 0;
    LONG index = InterlockedIncrement(&log_count);
    if (index <= 12) {
        log_kbo_allstar_native_event_state("schedule_import_capture", league_ptr, "schedule_import");
    }
}
