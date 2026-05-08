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
    }

    if (!verify_ootp_build()) {
        append_log_line("KBOFix: build verification failed; no patches installed");
        return 0;
    }

    if (diagnostic_minimal_runtime) {
        append_log_line("KBO diagnostic minimal runtime: build verified, no runtime patches installed");
        return 0;
    }

    if (read_kbo_localappdata_flag_file("enable_single_division_allstar_runtime_patches.txt")) {
        append_log_line("KBO all-star presave bootstrap install started");
        install_single_division_allstar_patch();
        install_allstar_team_setup_single_division_patch();
        install_allstar_candidate_team_split_patch();
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_voting_hook.txt")) {
            install_allstar_voting_begin_prepare_patch();
        } else {
            append_log_line("KBO all-star voting begin prepare hook disabled: kbo_flags.json enable_single_division_allstar_voting_hook is false");
        }
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_events.txt")) {
            install_allstar_events_prepare_patch();
        } else {
            append_log_line("KBO all-star events prepare hook disabled: kbo_flags.json enable_single_division_allstar_events is false");
        }
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_settings_patch.txt")) {
            install_allstar_settings_ui_patch();
        } else {
            append_log_line("KBO all-star settings UI patch disabled: kbo_flags.json enable_single_division_allstar_settings_patch is false");
        }
        append_log_line("KBO all-star presave bootstrap hooks installed");

        load_allstar_team_rules_once();
        patch_kbo_allstar_team_names_for_configured_league("startup");
        patch_kbo_allstar_team_names_for_known_exhibition_teams("startup");
        force_kbo_allstar_flags_for_configured_league("startup");
        start_kbo_allstar_force_retry_thread();
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_events.txt")) {
            force_kbo_allstar_flags_for_configured_league("startup_allstar_events_enabled");
        }
    } else {
        append_log_line("KBO single-division all-star runtime patches disabled: kbo_flags.json enable_single_division_allstar_runtime_patches is false");
        if (read_kbo_localappdata_flag_file("enable_single_division_allstar_settings_patch.txt")) {
            install_allstar_settings_ui_patch();
        } else {
            append_log_line("KBO all-star settings UI patch disabled: kbo_flags.json enable_single_division_allstar_settings_patch is false");
        }
    }

    if (read_kbo_localappdata_flag_file("disable_kbo_runtime_roster_marker_guard.txt")) {
        append_log_line("KBO runtime marker guard disabled by flag");
        install_kbo_full_runtime_after_roster_marker((HINSTANCE)parameter);
        return 0;
    }

    if (kbo_current_save_has_required_roster_marker("runtime_startup", 1)) {
        install_kbo_full_runtime_after_roster_marker((HINSTANCE)parameter);
    } else {
        start_kbo_full_runtime_marker_wait_thread((HINSTANCE)parameter);
    }
    return 0;
}
