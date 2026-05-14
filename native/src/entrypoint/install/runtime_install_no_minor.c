#include "../entrypoint_internal.h"
#include "../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../core/season/opening_day_storyline_guard.h"

static volatile LONG g_kbo_no_minor_contract_patch_install_started = 0;
static volatile LONG g_kbo_no_minor_contract_delayed_install_started = 0;

static int read_kbo_no_minor_contract_disable_flag(int* disabled)
{
    if (disabled == NULL) {
        return 0;
    }
    if (kbo_read_localappdata_json_flag_value(
            "disable_kbo_no_minor_contract_patch",
            "disable_kbo_no_minor_contract_patch.txt",
            disabled)) {
        return 1;
    }
    return kbo_read_localappdata_json_flag_value(
            "disable_kbo_no_minor_contract_experimental_patch",
            "disable_kbo_no_minor_contract_experimental_patch.txt",
            disabled);
}

int kbo_no_minor_contract_patch_enabled(void)
{
    int disabled = 0;
    if (!read_kbo_no_minor_contract_disable_flag(&disabled)) {
        append_log_line("KBO no-minor-contract patch enabled: disable flag missing, defaulting to enabled");
        return 1;
    }
    if (disabled) {
        append_log_line("KBO no-minor-contract patch disabled: kbo_flags.json disable_kbo_no_minor_contract_patch is true");
        return 0;
    }
    return 1;
}

int install_kbo_no_minor_contract_patch_once(const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_patch_install_started, 1, 0) != 0) {
        append_logf(
            "KBO no-minor-contract patch install skipped source=%s reason=already_started",
            source != NULL ? source : "");
        return 0;
    }

    append_logf(
        "KBO no-minor-contract patch installing source=%s",
        source != NULL ? source : "");
    int installed = install_kbo_no_minor_contract_patch();
    if (installed) {
        start_kbo_no_minor_contract_demand_floor_scanner_thread();
        append_log_line("KBO no-minor-contract demand-floor scanner enabled: safe RPM/WPM demand salary catch-up");
    }
    return installed;
}

DWORD WINAPI kbo_delayed_no_minor_contract_patch_install_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO no-minor-contract delayed install thread started");

    const KboRuntimeTuningPolicy* tuning = kbo_runtime_tuning_policy();
    for (int attempt = 1; attempt <= tuning->no_minor_delayed_install_attempts; attempt++) {
        uint32_t sleep_ms = attempt == 1
            ? (uint32_t)tuning->no_minor_delayed_install_first_sleep_ms
            : (uint32_t)tuning->no_minor_delayed_install_sleep_ms;
        if (!kbo_runtime_sleep_should_continue(sleep_ms)) {
            break;
        }

        if (!kbo_no_minor_contract_patch_enabled()) {
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
            if (attempt <= tuning->no_minor_delayed_install_log_initial_attempts
                    || attempt % tuning->no_minor_delayed_install_log_interval == 0) {
                append_logf(
                    "KBO no-minor-contract delayed install waiting attempt=%d reason=state_not_ready date_serial=%u save=%d",
                    attempt,
                    today_serial,
                    has_save);
            }
            continue;
        }

        if (kbo_opening_day_storyline_guard_active("no_minor_contract_delayed_install", NULL, NULL)) {
            if (attempt <= tuning->no_minor_delayed_install_log_initial_attempts
                    || attempt % tuning->no_minor_delayed_install_log_interval == 0) {
                append_logf(
                    "KBO no-minor-contract delayed install waiting attempt=%d reason=opening_day_storyline_guard save=%s",
                    attempt,
                    save_path);
            }
            continue;
        }

        int installed = install_kbo_no_minor_contract_patch_once("delayed_opening_day_guard");
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

void start_kbo_delayed_no_minor_contract_patch_install_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_delayed_install_started, 1, 0) != 0) {
        append_log_line("KBO no-minor-contract delayed install already started");
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_delayed_no_minor_contract_patch_install_thread,
            NULL,
            "delayed no-minor contract patch install")) {
        InterlockedExchange(&g_kbo_no_minor_contract_delayed_install_started, 0);
    }
}

void install_kbo_early_no_minor_contract_hooks_once(const char* source)
{
    if (!kbo_no_minor_contract_patch_enabled()) {
        append_logf(
            "KBO early no-minor-contract hooks skipped source=%s reason=disabled",
            source != NULL ? source : "");
        return;
    }

    append_logf(
        "KBO early no-minor-contract hooks deferred source=%s reason=avoid_presave_stock_storyline_side_effects",
        source != NULL ? source : "");
    start_kbo_delayed_no_minor_contract_patch_install_thread();
}
