static HANDLE g_kbo_process_instance_mutex = NULL;
static volatile LONG g_kbo_sangmu_fa_hooks_install_started = 0;

typedef struct KboSangmuFaHookInstallRequest {
    int enable_signability;
    int enable_offer;
    int enable_legacy;
} KboSangmuFaHookInstallRequest;

static DWORD WINAPI kbo_delayed_sangmu_fa_hooks_install_thread(LPVOID parameter)
{
    KboSangmuFaHookInstallRequest request = {0};
    if (parameter != NULL) {
        request = *(KboSangmuFaHookInstallRequest*)parameter;
        HeapFree(GetProcessHeap(), 0, parameter);
    }

    append_logf(
        "KBO Sangmu FA hooks delayed install waiting signability=%d offer=%d legacy=%d",
        request.enable_signability,
        request.enable_offer,
        request.enable_legacy);

    int stable_ticks = 0;
    for (int attempt = 1; attempt <= 180; attempt++) {
        char save_path[MAX_PATH] = {0};
        uint32_t today_serial = kbo_current_date_serial();
        int has_save = kbo_get_current_save_path(save_path, sizeof(save_path));
        if (has_save && today_serial != 0u) {
            stable_ticks++;
            if (stable_ticks >= 6) {
                break;
            }
        } else {
            stable_ticks = 0;
        }

        if (attempt == 1 || attempt == 10 || attempt == 30 || attempt == 60 || attempt == 120) {
            append_logf(
                "KBO Sangmu FA hooks delayed install not ready attempt=%d save=%d today=%u stable=%d",
                attempt,
                has_save,
                today_serial,
                stable_ticks);
        }
        Sleep(1000);
    }

    kbo_load_military_service_team_policy_override_once();

    if (request.enable_signability || request.enable_legacy) {
        install_kbo_player_team_signability_patch();
    } else {
        append_log_line("KBO player/team signability patch delayed disabled");
    }
    if (request.enable_offer || request.enable_legacy) {
        install_kbo_player_offer_eligibility_patch();
    } else {
        append_log_line("KBO player offer eligibility patch delayed disabled");
    }

    append_log_line("KBO Sangmu FA hooks delayed install complete");
    return 0;
}

static void start_kbo_delayed_sangmu_fa_hooks_install_thread(
    int enable_signability,
    int enable_offer,
    int enable_legacy)
{
    if (!enable_signability && !enable_offer && !enable_legacy) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_sangmu_fa_hooks_install_started, 1, 0) != 0) {
        append_log_line("KBO Sangmu FA hooks delayed install already started");
        return;
    }

    KboSangmuFaHookInstallRequest* request =
        (KboSangmuFaHookInstallRequest*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(KboSangmuFaHookInstallRequest));
    if (request == NULL) {
        append_log_line("KBO Sangmu FA hooks delayed install request allocation failed");
        return;
    }
    request->enable_signability = enable_signability;
    request->enable_offer = enable_offer;
    request->enable_legacy = enable_legacy;

    HANDLE thread = CreateThread(NULL, 0, kbo_delayed_sangmu_fa_hooks_install_thread, request, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("KBO Sangmu FA hooks delayed install thread started");
    } else {
        append_logf("KBO Sangmu FA hooks delayed install thread failed error=%lu", GetLastError());
        HeapFree(GetProcessHeap(), 0, request);
    }
}

static DWORD WINAPI patch_thread(LPVOID parameter)
{
    append_log_line("KBOFix loaded");

    if (!read_kbo_localappdata_flag_file("enable_experimental_runtime_hooks.txt")) {
        append_log_line("KBOFix: experimental runtime hooks disabled; safe startup mode active");
        return 0;
    }

    int diagnostic_minimal_runtime = read_kbo_localappdata_flag_file("enable_kbo_diagnostic_minimal_runtime.txt");
    if (diagnostic_minimal_runtime) {
        append_log_line("KBO diagnostic minimal runtime enabled: F2 hub and runtime patches disabled");
    } else {
        start_kbo_hotkey_window_thread((HINSTANCE)parameter);
    }

    if (!verify_ootp_build()) {
        append_log_line("KBOFix: build verification failed; no patches installed");
        return 0;
    }

    if (diagnostic_minimal_runtime) {
        append_log_line("KBO diagnostic minimal runtime: build verified, no runtime patches installed");
        return 0;
    }

    load_allstar_team_rules_once();
    install_kbo_military_service_entry_patch();
    install_kbo_military_status_update_patch();
    if (read_kbo_localappdata_flag_file("enable_single_division_allstar_runtime_patches.txt")) {
        force_kbo_allstar_flags_for_configured_league("startup");
        start_kbo_allstar_force_retry_thread();
        install_single_division_allstar_patch();
        install_allstar_team_setup_single_division_patch();
        install_allstar_candidate_team_split_patch();
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_voting_hook.txt")) {
            install_allstar_voting_begin_prepare_patch();
        } else {
            append_log_line("KBO all-star voting begin prepare hook disabled: kbo_flags.json enable_single_division_allstar_voting_hook is false");
        }
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_events.txt")) {
            force_kbo_allstar_flags_for_configured_league("startup_allstar_events_enabled");
            install_allstar_events_prepare_patch();
        } else {
            append_log_line("KBO all-star events prepare hook disabled: kbo_flags.json enable_single_division_allstar_events is false");
        }
    } else {
        append_log_line("KBO single-division all-star runtime patches disabled: kbo_flags.json enable_single_division_allstar_runtime_patches is false");
    }
    if (read_kbo_localappdata_flag_file("enable_single_division_allstar_settings_patch.txt")) {
        install_allstar_settings_ui_patch();
    } else {
        append_log_line("KBO all-star settings UI patch disabled: kbo_flags.json enable_single_division_allstar_settings_patch is false");
    }
    int enable_legacy_fa_signability_hooks = read_kbo_localappdata_flag_file("enable_kbo_fa_signability_hooks.txt");
    kbo_load_military_service_team_policy_override_once();
    int enable_sangmu_fa_block_default = !read_kbo_localappdata_flag_file("disable_kbo_sangmu_fa_block_core.txt");
    int enable_sangmu_signability_only = enable_sangmu_fa_block_default
        ||
        read_kbo_localappdata_flag_file("enable_kbo_sangmu_signability_only.txt");
    int enable_sangmu_offer_only = enable_sangmu_fa_block_default
        || read_kbo_localappdata_flag_file("enable_kbo_sangmu_offer_only.txt");
    int enable_sangmu_fa_block_core = enable_sangmu_signability_only || enable_sangmu_offer_only;
    if (enable_sangmu_fa_block_core) {
        append_logf(
            "KBO Sangmu FA block enabled: signability=%d offer=%d",
            enable_sangmu_signability_only,
            enable_sangmu_offer_only);
    } else {
        append_log_line("KBO Sangmu FA block diagnostic disabled: kbo_flags.json enable_kbo_sangmu_signability_only / enable_kbo_sangmu_offer_only are false");
    }
    start_kbo_delayed_sangmu_fa_hooks_install_thread(
        enable_sangmu_signability_only || read_kbo_localappdata_flag_file("enable_kbo_player_team_signability_patch.txt"),
        enable_sangmu_offer_only || read_kbo_localappdata_flag_file("enable_kbo_offer_eligibility_patch.txt"),
        enable_legacy_fa_signability_hooks);
    if (enable_legacy_fa_signability_hooks || read_kbo_localappdata_flag_file("enable_kbo_ai_fa_fallback_patch.txt")) {
        install_kbo_ai_fa_signability_random_fallback_patch();
    } else {
        append_log_line("KBO AI FA signability fallback patch disabled: kbo_flags.json enable_kbo_ai_fa_fallback_patch is false");
    }
    if (enable_legacy_fa_signability_hooks || read_kbo_localappdata_flag_file("enable_kbo_submit_offer_probe_patch.txt")) {
        install_kbo_fa_submit_offer_probe_patch();
    } else {
        append_log_line("KBO FA submit-offer probe patch disabled: kbo_flags.json enable_kbo_submit_offer_probe_patch is false");
    }
    if (read_kbo_localappdata_flag_file("enable_fa_requalification.txt")) {
        install_kbo_fa_signing_branch_patch();
    } else {
        append_log_line("KBO FA requalification signing hook disabled: kbo_flags.json enable_fa_requalification is false");
    }
    if (!read_kbo_localappdata_flag_file("disable_kbo_ai_fa_status_candidate_insert_hook.txt")) {
        install_kbo_ai_fa_status_candidate_insert_patch();
    } else {
        append_log_line("KBO AI FA status candidate insert hook disabled: kbo_flags.json disable_kbo_ai_fa_status_candidate_insert_hook is true");
    }
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
        append_log_line("KBO FA requalification thread disabled: kbo_flags.json enable_fa_requalification is false");
    }
    start_kbo_custom_event_monitor();
    if (read_kbo_localappdata_flag_file("enable_kbo_season_phase_monitor.txt")) {
        start_kbo_season_phase_monitor();
        append_log_line("KBO season phase write probe disabled: unsafe after crash; using read-only monitor");
    } else {
        append_log_line("KBO season phase read-only monitor disabled: kbo_flags.json enable_kbo_season_phase_monitor is false");
    }
    start_kbo_military_seed_bootstrap_thread();
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
