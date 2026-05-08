static DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO military service day tick thread started");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }
        tick_kbo_military_service_days("military_days_tick", NULL);
    }
    InterlockedExchange(&g_military_days_tick_started, 0);
    append_log_line("KBO military service day tick thread stopped");
    return 0;
}

static DWORD WINAPI kbo_military_seed_bootstrap_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO military service seed bootstrap thread started");

    char last_save_path[MAX_PATH] = {0};
    int settled_attempts = 0;
    for (int attempt = 1; attempt <= 36; attempt++) {
        if (!kbo_runtime_sleep_should_continue(attempt == 1 ? 2500u : 5000u)) {
            break;
        }

        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            if (attempt <= 8 || attempt % 6 == 0) {
                append_logf("KBO military service seed bootstrap waiting attempt=%d reason=no_save_path", attempt);
            }
            continue;
        }

        if (_stricmp(last_save_path, save_path) != 0) {
            snprintf(last_save_path, sizeof(last_save_path), "%s", save_path);
            settled_attempts = 0;
        }

        uint32_t today_serial = kbo_current_date_serial();
        uintptr_t player_vector = 0;
        int32_t player_count = 0;
        uint32_t vector_offset = 0;
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb = find_kbo_team_by_csv_id_any_league("KPB", 0);
        if (today_serial == 0u
                || !find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)
                || (sang == NULL && kpb == NULL)) {
            if (attempt <= 8 || attempt % 6 == 0) {
                append_logf(
                    "KBO military service seed bootstrap waiting attempt=%d reason=state_not_ready date_serial=%u player_count=%d sang=%p kpb=%p save=%s",
                    attempt,
                    today_serial,
                    player_count,
                    (void*)sang,
                    (void*)kpb,
                    save_path);
            }
            continue;
        }

        int seeded = 0;
        int returned = tick_kbo_military_service_days("military_seed_bootstrap", &seeded);
        if (seeded > 0 || returned > 0) {
            append_logf(
                "KBO military service seed bootstrap applied attempt=%d seeded=%d returned=%d save=%s",
                attempt,
                seeded,
                returned,
                save_path);
            return 0;
        }

        if (GetFileAttributesA(save_path) != INVALID_FILE_ATTRIBUTES) {
            settled_attempts++;
        }
        if (settled_attempts >= 3) {
            append_logf(
                "KBO military service seed bootstrap settled attempt=%d seeded=0 returned=0 save=%s",
                attempt,
                save_path);
            return 0;
        }
    }

    append_log_line("KBO military service seed bootstrap ended without settled save");
    return 0;
}

static void start_kbo_military_seed_bootstrap_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_seed_bootstrap_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_military_seed_bootstrap_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_military_seed_bootstrap_started, 0);
        append_log_line("KBO military service seed bootstrap thread failed to start");
    }
}

static void start_kbo_military_days_tick_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_days_tick_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_military_days_tick_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_military_days_tick_started, 0);
        append_log_line("KBO military service day tick thread failed to start");
    }
}
