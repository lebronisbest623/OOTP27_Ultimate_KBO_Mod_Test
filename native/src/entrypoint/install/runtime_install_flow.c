#include "../entrypoint_internal.h"
#include "../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../core/season/opening_day_storyline_guard.h"

static int kbo_no_minor_contract_experimental_patch_enabled(void)
{
    int disabled = 1;
    if (!kbo_read_localappdata_json_flag_value(
            "disable_kbo_no_minor_contract_experimental_patch",
            "disable_kbo_no_minor_contract_experimental_patch.txt",
            &disabled)) {
        append_log_line("KBO no-minor-contract experimental patch disabled: missing explicit recovery flag defaults to safe-off");
        return 0;
    }
    if (disabled) {
        append_log_line("KBO no-minor-contract experimental patch disabled: kbo_flags.json disable_kbo_no_minor_contract_experimental_patch is true");
        return 0;
    }
    return 1;
}

static volatile LONG g_kbo_no_minor_contract_patch_install_started = 0;
static volatile LONG g_kbo_no_minor_contract_delayed_install_started = 0;

static int install_kbo_no_minor_contract_experimental_patch_once(const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_patch_install_started, 1, 0) != 0) {
        append_logf(
            "KBO no-minor-contract experimental patch install skipped source=%s reason=already_started",
            source != NULL ? source : "");
        return 0;
    }

    append_logf(
        "KBO no-minor-contract experimental patch installing source=%s",
        source != NULL ? source : "");
    int installed = install_kbo_no_minor_contract_experimental_patch();
    start_kbo_no_minor_contract_demand_floor_scanner_thread();
    return installed;
}

DWORD WINAPI kbo_delayed_no_minor_contract_patch_install_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO no-minor-contract delayed install thread started");

    for (int attempt = 1; attempt <= 720; attempt++) {
        if (!kbo_runtime_sleep_should_continue(attempt == 1 ? 1000u : 5000u)) {
            break;
        }

        if (!kbo_no_minor_contract_experimental_patch_enabled()) {
            append_logf(
                "KBO no-minor-contract delayed install ended attempt=%d reason=disabled",
                attempt);
            InterlockedExchange(&g_kbo_no_minor_contract_delayed_install_started, 0);
            return 0;
        }

        char save_path[MAX_PATH] = {0};
        uint32_t today_serial = kbo_current_date_serial();
        int has_save = kbo_get_current_save_path(save_path, sizeof(save_path));
        if (today_serial == 0u || !has_save) {
            if (attempt <= 8 || attempt % 12 == 0) {
                append_logf(
                    "KBO no-minor-contract delayed install waiting attempt=%d reason=state_not_ready date_serial=%u save=%d",
                    attempt,
                    today_serial,
                    has_save);
            }
            continue;
        }

        if (kbo_opening_day_storyline_guard_active("no_minor_contract_delayed_install", NULL, NULL)) {
            if (attempt <= 8 || attempt % 12 == 0) {
                append_logf(
                    "KBO no-minor-contract delayed install waiting attempt=%d reason=opening_day_storyline_guard save=%s",
                    attempt,
                    save_path);
            }
            continue;
        }

        int installed = install_kbo_no_minor_contract_experimental_patch_once("delayed_opening_day_guard");
        append_logf(
            "KBO no-minor-contract delayed install complete attempt=%d installed_any=%d save=%s",
            attempt,
            installed,
            save_path);
        return 0;
    }

    InterlockedExchange(&g_kbo_no_minor_contract_delayed_install_started, 0);
    append_log_line("KBO no-minor-contract delayed install ended without installing");
    return 0;
}

static void start_kbo_delayed_no_minor_contract_patch_install_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_delayed_install_started, 1, 0) != 0) {
        append_log_line("KBO no-minor-contract delayed install already started");
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_delayed_no_minor_contract_patch_install_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "delayed no-minor contract patch install");
    } else {
        InterlockedExchange(&g_kbo_no_minor_contract_delayed_install_started, 0);
        append_logf("KBO no-minor-contract delayed install thread failed error=%lu", GetLastError());
    }
}

DWORD WINAPI kbo_delayed_sangmu_fa_hooks_install_thread(LPVOID parameter)
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

void start_kbo_delayed_sangmu_fa_hooks_install_thread(
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
        kbo_register_runtime_thread(thread, "delayed sangmu FA hook install");
        append_log_line("KBO Sangmu FA hooks delayed install thread started");
    } else {
        append_logf("KBO Sangmu FA hooks delayed install thread failed error=%lu", GetLastError());
        HeapFree(GetProcessHeap(), 0, request);
    }
}

static int kbo_current_save_has_completed_flag(const char* save_path, const char* source, int log_detail)
{
    char completed_path[MAX_PATH] = {0};
    int path_written = snprintf(completed_path, sizeof(completed_path), "%s\\flag_save_completed.dat", save_path);
    if (path_written <= 0 || (size_t)path_written >= sizeof(completed_path)) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=save_completed_path_overflow save=%s",
                source != NULL ? source : "",
                save_path != NULL ? save_path : "");
        }
        return 0;
    }

    HANDLE file = CreateFileA(
        completed_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=save_not_completed gle=%lu path=%s",
                source != NULL ? source : "",
                GetLastError(),
                completed_path);
        }
        return 0;
    }

    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 1024 * 1024) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=save_completed_size_invalid path=%s",
                source != NULL ? source : "",
                completed_path);
        }
        CloseHandle(file);
        return 0;
    }

    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size.QuadPart + 1u);
    if (text == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL read_ok = ReadFile(file, text, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!read_ok || read != (DWORD)size.QuadPart) {
        HeapFree(GetProcessHeap(), 0, text);
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=save_completed_read_failed path=%s",
                source != NULL ? source : "",
                completed_path);
        }
        return 0;
    }
    text[read] = '\0';

    int completed = kbo_ascii_contains_ignore_case(text, "Finished save_database, closing flag file now");
    HeapFree(GetProcessHeap(), 0, text);
    if (!completed) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=save_not_completed path=%s",
                source != NULL ? source : "",
                completed_path);
        }
        return 0;
    }

    return 1;
}

int kbo_current_save_has_required_roster_marker(const char* source, int log_detail)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        if (log_detail) {
            append_logf("KBO runtime marker guard waiting source=%s reason=current_save_unavailable", source != NULL ? source : "");
        }
        return 0;
    }

    char description_path[MAX_PATH] = {0};
    int path_written = snprintf(description_path, sizeof(description_path), "%s\\description.txt", save_path);
    if (path_written <= 0 || (size_t)path_written >= sizeof(description_path)) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_path_overflow save=%s",
                source != NULL ? source : "",
                save_path);
        }
        return 0;
    }

    HANDLE file = CreateFileA(
        description_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_unavailable gle=%lu path=%s",
                source != NULL ? source : "",
                GetLastError(),
                description_path);
        }
        return 0;
    }

    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 1024 * 1024) {
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_size_invalid path=%s",
                source != NULL ? source : "",
                description_path);
        }
        CloseHandle(file);
        return 0;
    }

    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size.QuadPart + 1u);
    if (text == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL read_ok = ReadFile(file, text, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!read_ok || read != (DWORD)size.QuadPart) {
        HeapFree(GetProcessHeap(), 0, text);
        if (log_detail) {
            append_logf(
                "KBO runtime marker guard waiting source=%s reason=description_read_failed path=%s",
                source != NULL ? source : "",
                description_path);
        }
        return 0;
    }
    text[read] = '\0';

    int marked = kbo_ascii_contains_ignore_case(text, KBO_REQUIRED_ROSTER_MARKER_URL);
    HeapFree(GetProcessHeap(), 0, text);
    if (marked) {
        if (!kbo_current_save_has_completed_flag(save_path, source, log_detail)) {
            return 0;
        }
        append_logf(
            "KBO runtime marker guard passed source=%s save=%s description=%s save_completed=%s\\flag_save_completed.dat",
            source != NULL ? source : "",
            save_path,
            description_path,
            save_path);
        return 1;
    }

    if (log_detail) {
        append_logf(
            "KBO runtime marker guard waiting source=%s reason=marker_missing save=%s description=%s",
            source != NULL ? source : "",
            save_path,
            description_path);
    }
    return 0;
}

void install_kbo_full_runtime_after_roster_marker(HINSTANCE instance)
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
    int enable_ai_fa_status_candidate_insert_hook =
        read_kbo_localappdata_flag_file("enable_kbo_ai_fa_status_candidate_insert_hook.txt");
    append_logf(
        "KBO FA market diagnostic flags: enable_ai_fa_status_candidate_insert_hook=%d disable_intl_established_fa_generation_filter=%d",
        enable_ai_fa_status_candidate_insert_hook,
        read_kbo_localappdata_flag_file("disable_intl_established_fa_generation_filter.txt"));
    if (enable_ai_fa_status_candidate_insert_hook) {
        install_kbo_ai_fa_status_candidate_insert_patch();
    } else {
        append_log_line("KBO AI FA status candidate insert hook disabled: defaulting to native FA market candidate insertion");
    }
    if (kbo_no_minor_contract_experimental_patch_enabled()) {
        if (!kbo_opening_day_storyline_guard_active("no_minor_contract_patch_install", NULL, NULL)) {
            install_kbo_no_minor_contract_experimental_patch_once("runtime_install");
        } else {
            append_log_line("KBO no-minor-contract experimental patch deferred during opening-day stock-news guard");
            start_kbo_delayed_no_minor_contract_patch_install_thread();
        }
    }
    if (!read_kbo_localappdata_flag_file("disable_kbo_salary_arbitration_no_withdraw_patch.txt")) {
        install_kbo_salary_arbitration_no_withdraw_patch();
    } else {
        append_log_line("KBO salary arbitration no-withdraw patch disabled: kbo_flags.json disable_kbo_salary_arbitration_no_withdraw_patch is true");
    }
    install_kbo_intl_established_fa_multiplier_patch();
    if (!read_kbo_localappdata_flag_file("disable_intl_established_fa_generation_filter.txt")) {
        install_kbo_intl_established_fa_generation_filter_patch();
    } else {
        append_log_line("KBO international established FA generation filter patch disabled: kbo_flags.json disable_intl_established_fa_generation_filter is true");
    }
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
        append_log_line("KBO FA salary opening-day phase hook retired: using league-memory/news snapshot thread only");
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

DWORD WINAPI kbo_full_runtime_marker_wait_thread(LPVOID parameter)
{
    HINSTANCE instance = (HINSTANCE)parameter;
    append_log_line("KBO full runtime marker guard thread started");

    uint32_t last_date_serial = 0u;
    int stable_date_ticks = 0;
    for (int attempt = 1; attempt <= 450; attempt++) {
        int log_detail = attempt == 1 || attempt == 5 || attempt == 15 || attempt % 30 == 0;
        if (kbo_current_save_has_required_roster_marker("runtime_marker_wait", log_detail)) {
            uint32_t today_serial = kbo_current_date_serial();
            if (today_serial != 0u && today_serial == last_date_serial) {
                stable_date_ticks++;
            } else if (today_serial != 0u) {
                last_date_serial = today_serial;
                stable_date_ticks = 1;
            } else {
                last_date_serial = 0u;
                stable_date_ticks = 0;
            }

            if (stable_date_ticks >= 8) {
                install_kbo_full_runtime_after_roster_marker(instance);
                return 0;
            }

            if (log_detail || stable_date_ticks == 1) {
                append_logf(
                    "KBO full runtime marker guard waiting source=runtime_marker_wait reason=current_date_not_stable date_serial=%u stable_ticks=%d",
                    today_serial,
                    stable_date_ticks);
            }
        } else {
            last_date_serial = 0u;
            stable_date_ticks = 0;
        }
        Sleep(2000);
    }

    append_log_line("KBO full runtime marker guard gave up: required roster marker was not found");
    return 0;
}

