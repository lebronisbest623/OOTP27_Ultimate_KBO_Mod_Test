static HANDLE g_kbo_process_instance_mutex = NULL;

static DWORD WINAPI patch_thread(LPVOID parameter)
{
    append_log_line("KBOFix loaded");

    if (!read_kbo_localappdata_flag_file("enable_experimental_runtime_hooks.txt")) {
        append_log_line("KBOFix: experimental runtime hooks disabled; safe startup mode active");
        return 0;
    }

    start_kbo_hotkey_window_thread((HINSTANCE)parameter);

    if (!verify_ootp_build()) {
        append_log_line("KBOFix: build verification failed; no patches installed");
        return 0;
    }

    load_allstar_team_rules_once();
    force_kbo_allstar_flags_for_configured_league("startup");
    start_kbo_allstar_force_retry_thread();
    install_kbo_military_service_entry_patch();
    install_kbo_military_status_update_patch();
    install_single_division_allstar_patch();
    install_allstar_team_setup_single_division_patch();
    install_allstar_voting_begin_prepare_patch();
    install_allstar_candidate_team_split_patch();
    if (read_kbo_localappdata_flag_file("enable_single_division_allstar_events.txt")) {
        force_kbo_allstar_flags_for_configured_league("startup_allstar_events_enabled");
        install_allstar_events_prepare_patch();
    } else {
        append_log_line("KBO all-star events prepare hook disabled: enable_single_division_allstar_events.txt is off");
    }
    install_allstar_settings_ui_patch();
    install_kbo_player_team_signability_patch();
    install_kbo_player_offer_eligibility_patch();
    install_kbo_ai_fa_signability_random_fallback_patch();
    install_kbo_fa_submit_offer_probe_patch();
    if (read_kbo_localappdata_flag_file("enable_fa_requalification.txt")) {
        install_kbo_fa_signing_branch_patch();
    } else {
        append_log_line("KBO FA requalification signing hook disabled: enable_fa_requalification.txt is off");
    }
    install_kbo_ai_fa_status_candidate_insert_patch();
    install_kbo_foreign_count_patches();
    if (read_kbo_localappdata_flag_file("enable_kbo_callup_foreign_limit_patch.txt")) {
        install_kbo_callup_foreign_limit_branch_patches();
    } else {
        append_log_line("KBO callup foreign limit branch patches disabled: custom foreign policy is org-level");
    }
    start_kbo_foreign_waiver_scanner_thread();
    start_kbo_foreign_injury_replacement_thread();
    if (read_kbo_localappdata_flag_file("enable_fa_requalification.txt")) {
        start_kbo_fa_requalification_thread();
    } else {
        append_log_line("KBO FA requalification thread disabled: enable_fa_requalification.txt is off");
    }
    start_kbo_custom_event_monitor();
    start_kbo_military_days_tick_thread();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);

        char mutex_name[96] = {0};
        snprintf(
            mutex_name,
            sizeof(mutex_name),
            "Local\\OOTP_KBO_FIX_%lu",
            (unsigned long)GetCurrentProcessId());
        g_kbo_process_instance_mutex = CreateMutexA(NULL, TRUE, mutex_name);
        if (g_kbo_process_instance_mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
            return TRUE;
        }

        HANDLE thread = CreateThread(NULL, 0, patch_thread, instance, 0, NULL);
        if (thread != NULL) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_kbo_process_instance_mutex != NULL) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
        }
    }

    return TRUE;
}
