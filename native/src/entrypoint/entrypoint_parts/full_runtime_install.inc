static void install_kbo_full_runtime_after_roster_marker(HINSTANCE instance)
{
    if (InterlockedCompareExchange(&g_kbo_full_runtime_install_started, 1, 0) != 0) {
        append_log_line("KBO full runtime install skipped: already started");
        return;
    }

    start_kbo_hotkey_window_thread(instance);

    install_kbo_military_service_entry_patch();
    install_kbo_military_status_update_patch();
    int enable_legacy_fa_signability_hooks = read_kbo_localappdata_flag_file("enable_kbo_fa_signability_hooks.txt");
    kbo_load_military_service_team_policy_override_once();
    int enable_sangmu_fa_block_default = !read_kbo_localappdata_flag_file("disable_kbo_sangmu_fa_block_core.txt");
    int enable_sangmu_signability_only = enable_sangmu_fa_block_default
        ||
        read_kbo_localappdata_flag_file("enable_kbo_sangmu_signability_only.txt");
    int enable_sangmu_offer_only = enable_sangmu_fa_block_default
        || read_kbo_localappdata_flag_file("enable_kbo_sangmu_offer_only.txt");
    int enable_sangmu_fa_block_core = enable_sangmu_signability_only || enable_sangmu_offer_only;
    int enable_fa_compensation_core = !read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt");
    int enable_amateur_assignment_core =
        !read_kbo_localappdata_flag_file("disable_amateur_assignment_reroute.txt");
    if (enable_sangmu_fa_block_core) {
        append_logf(
            "KBO Sangmu FA block enabled: signability=%d offer=%d",
            enable_sangmu_signability_only,
            enable_sangmu_offer_only);
    } else {
        append_log_line("KBO Sangmu FA block diagnostic disabled: kbo_flags.json enable_kbo_sangmu_signability_only / enable_kbo_sangmu_offer_only are false");
    }
    if (enable_sangmu_fa_block_core || enable_fa_compensation_core || enable_amateur_assignment_core) {
        if (!read_kbo_localappdata_flag_file("disable_kbo_military_team_add_guard_patch.txt")) {
            install_kbo_military_team_add_guard_patch();
        } else {
            append_log_line("KBO military team-add guard patch disabled: kbo_flags.json disable_kbo_military_team_add_guard_patch is true");
        }
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
    int enable_submit_offer_probe = enable_legacy_fa_signability_hooks
        || read_kbo_localappdata_flag_file("enable_kbo_submit_offer_probe_patch.txt")
        || (kbo_custom_foreign_policy_enabled()
            && !read_kbo_localappdata_flag_file("disable_kbo_submit_offer_probe_patch.txt"));
    if (enable_submit_offer_probe) {
        install_kbo_fa_submit_offer_probe_patch();
    } else {
        append_log_line("KBO FA submit-offer probe patch disabled: kbo_flags.json disable_kbo_submit_offer_probe_patch is true");
    }
    int enable_fa_requalification = read_kbo_localappdata_flag_file("enable_fa_requalification.txt");
    int enable_fa_signing_branch = enable_fa_requalification
        || enable_sangmu_fa_block_core
        || enable_fa_compensation_core
        || (kbo_custom_foreign_policy_enabled()
            && !read_kbo_localappdata_flag_file("disable_kbo_foreign_signing_branch_patch.txt"));
    if (enable_fa_signing_branch) {
        install_kbo_fa_signing_branch_patch();
    } else {
        append_log_line("KBO FA signing branch hook disabled: enable_fa_requalification is false, Sangmu FA block is disabled, and custom foreign policy hook is disabled");
    }
    if (kbo_custom_foreign_policy_enabled()
            && read_kbo_localappdata_flag_file("enable_kbo_foreign_trade_check_patch.txt")
            && !read_kbo_localappdata_flag_file("disable_kbo_foreign_trade_check_patch.txt")) {
        install_kbo_trade_check_foreign_policy_patch();
    } else {
        append_log_line("KBO trade foreign policy patch disabled: custom foreign policy is disabled, enable_kbo_foreign_trade_check_patch is false, or disable_kbo_foreign_trade_check_patch is true");
    }
    if (!read_kbo_localappdata_flag_file("disable_kbo_ai_fa_status_candidate_insert_hook.txt")) {
        install_kbo_ai_fa_status_candidate_insert_patch();
    } else {
        append_log_line("KBO AI FA status candidate insert hook disabled: kbo_flags.json disable_kbo_ai_fa_status_candidate_insert_hook is true");
    }
    if (!read_kbo_localappdata_flag_file("disable_kbo_no_minor_contract_experimental_patch.txt")) {
        install_kbo_no_minor_contract_experimental_patch();
        start_kbo_no_minor_contract_demand_floor_scanner_thread();
    } else {
        append_log_line("KBO no-minor-contract experimental patch disabled: kbo_flags.json disable_kbo_no_minor_contract_experimental_patch is true");
    }
    if (!read_kbo_localappdata_flag_file("disable_kbo_salary_arbitration_no_withdraw_patch.txt")) {
        install_kbo_salary_arbitration_no_withdraw_patch();
    } else {
        append_log_line("KBO salary arbitration no-withdraw patch disabled: kbo_flags.json disable_kbo_salary_arbitration_no_withdraw_patch is true");
    }
    install_kbo_intl_established_fa_multiplier_patch();
    install_kbo_intl_established_fa_generation_filter_patch();
    start_kbo_intl_established_fa_postscan_thread();
    if (read_kbo_localappdata_flag_file("enable_intl_established_fa_quality_probe_patch.txt")) {
        install_kbo_intl_established_fa_player_probe_patch();
    } else {
        append_log_line("KBO international established FA player probe patch disabled: kbo_flags.json enable_intl_established_fa_quality_probe_patch is false");
    }
    install_kbo_foreign_count_patches();
    if (read_kbo_localappdata_flag_file("enable_kbo_callup_foreign_limit_patch.txt")) {
        install_kbo_callup_foreign_limit_branch_patches();
    } else {
        append_log_line("KBO callup foreign limit branch patches disabled: custom foreign policy is org-level");
    }
    start_kbo_foreign_waiver_scanner_thread();
    start_kbo_foreign_injury_replacement_thread();
    if (!read_kbo_localappdata_flag_file("disable_kbo_fa_salary_opening_day_snapshot.txt")) {
        start_kbo_fa_salary_snapshot_phase_event_thread();
        if (!read_kbo_localappdata_flag_file("disable_kbo_fa_salary_opening_day_phase_hook.txt")) {
            install_kbo_season_phase_opening_day_snapshot_hooks();
        } else {
            append_log_line("KBO FA salary opening-day phase hook disabled: kbo_flags.json disable_kbo_fa_salary_opening_day_phase_hook is true");
        }
        start_kbo_fa_salary_snapshot_thread();
    } else {
        append_log_line("KBO FA salary opening-day snapshot thread disabled: kbo_flags.json disable_kbo_fa_salary_opening_day_snapshot is true");
    }
    if (enable_fa_requalification) {
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
    append_log_line("KBO full runtime install complete after roster marker");
}
