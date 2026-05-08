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
