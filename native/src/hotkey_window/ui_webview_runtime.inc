typedef HRESULT (STDAPICALLTYPE *KboCreateCoreWebView2EnvironmentWithOptionsFn)(
    PCWSTR,
    PCWSTR,
    ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

typedef struct KboWebViewEnvHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewEnvHandler;

typedef struct KboWebViewControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewControllerHandler;

typedef struct KboWebViewNavHandler {
    ICoreWebView2NavigationStartingEventHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewNavHandler;

static HRESULT STDMETHODCALLTYPE kbo_webview_nav_qi(ICoreWebView2NavigationStartingEventHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2NavigationStartingEventHandler)) {
        *ppv = This;
        ICoreWebView2NavigationStartingEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_addref(ICoreWebView2NavigationStartingEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewNavHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_release(ICoreWebView2NavigationStartingEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewNavHandler*)This)->ref);
    if (value < 1) { ((KboWebViewNavHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewNavHandler*)This)->ref;
}

static int kbo_webview_handle_command_uri(const char* uri, HWND hwnd)
{
    if (uri == NULL || strncmp(uri, "kbo://", 6) != 0) {
        return 0;
    }
    const char* cmd = uri + 6;
    if (ascii_equals_ignore_case(cmd, "github")) {
        ShellExecuteA(
            hwnd,
            "open",
            "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO",
            NULL,
            NULL,
            SW_SHOWNORMAL);
        return 1;
    }
    if (strncmp(cmd, "select/", 7) == 0) {
        uint32_t player_id = (uint32_t)strtoul(cmd + 7, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            append_logf("foreign rights webview: selected player=%u", player_id);
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "retain/", 7) == 0 || strncmp(cmd, "release/", 8) == 0) {
        int retain = strncmp(cmd, "retain/", 7) == 0;
        const char* id_text = retain ? cmd + 7 : cmd + 8;
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            kbo_apply_foreign_rights_button(retain);
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "view/", 5) == 0) {
        int view = atoi(cmd + 5);
        if (view >= 0 && view < KBO_HUB_NAV_COUNT) {
            if (view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_RIGHTS;
            } else if (view == KBO_HUB_VIEW_UPCOMING_FA) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            } else {
                g_kbo_hub_selected_view = view;
            }
            g_kbo_hub_open_dropdown = 0;
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
                    && (g_kbo_hub_selected_foreign_subview < 0
                        || g_kbo_hub_selected_foreign_subview >= KBO_HUB_FOREIGN_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_ASIAN_GAMES
                    && (g_kbo_hub_selected_agames_subview < 0
                        || g_kbo_hub_selected_agames_subview >= KBO_HUB_AGAMES_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_agames_subview = KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS;
            }
            if (view == KBO_HUB_VIEW_MILITARY
                    && (g_kbo_hub_selected_military_subview < 0
                        || g_kbo_hub_selected_military_subview >= KBO_HUB_MILITARY_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_MOD_INFO
                    && (g_kbo_hub_selected_mod_subview < 0
                        || g_kbo_hub_selected_mod_subview >= KBO_HUB_MOD_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_README;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
                    && (g_kbo_hub_selected_fa_subview < 0
                        || g_kbo_hub_selected_fa_subview >= KBO_HUB_FA_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            }
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/results/year/", 22) == 0) {
        uint32_t year = (uint32_t)strtoul(cmd + 22, NULL, 10);
        if (year >= 1982u && year <= 2300u) {
            g_kbo_hub_selected_military_results_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
        g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_RESULTS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/", 9) == 0) {
        int subview = atoi(cmd + 9);
        if (subview >= 0 && subview < KBO_HUB_MILITARY_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
            g_kbo_hub_selected_military_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "foreign/", 8) == 0) {
        int subview = atoi(cmd + 8);
        if (subview >= 0 && subview < KBO_HUB_FOREIGN_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
            g_kbo_hub_selected_foreign_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/lang/", 18) == 0) {
        const char* lang = cmd + 18;
        if (ascii_equals_ignore_case(lang, "en") || ascii_equals_ignore_case(lang, "english")) {
            g_kbo_hub_language = KBO_HUB_LANG_EN;
            kbo_hub_save_language_setting();
        } else if (ascii_equals_ignore_case(lang, "ko") || ascii_equals_ignore_case(lang, "korean")) {
            g_kbo_hub_language = KBO_HUB_LANG_KO;
            kbo_hub_save_language_setting();
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/", 4) == 0) {
        int subview = atoi(cmd + 4);
        if (subview >= 0 && subview < KBO_HUB_MOD_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_selected_mod_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "agames/", 7) == 0) {
        int subview = atoi(cmd + 7);
        if (subview >= 0 && subview < KBO_HUB_AGAMES_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_GAMES;
            g_kbo_hub_selected_agames_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa/", 3) == 0) {
        int subview = atoi(cmd + 3);
        if (subview >= 0 && subview < KBO_HUB_FA_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
            g_kbo_hub_selected_fa_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/select/", 15) == 0) {
        const char* text = cmd + 15;
        uint32_t fa_player_id = (uint32_t)strtoul(text, NULL, 10);
        const char* slash = strchr(text, '/');
        uint32_t selected_player_id = slash != NULL ? (uint32_t)strtoul(slash + 1, NULL, 10) : 0u;
        if (fa_player_id != 0u && selected_player_id != 0u) {
            kbo_manual_select_fa_compensation_player(fa_player_id, selected_player_id, "hub_manual_compensation_select");
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/cash-only/", 18) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 18, NULL, 10);
        if (fa_player_id != 0u) {
            kbo_manual_select_fa_compensation_cash_only(fa_player_id, "hub_cash_only_select");
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/submit/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            kbo_manual_submit_fa_compensation_protected_list(fa_player_id, "hub_protected_list_submit");
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/detail/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/intl-fa-multiplier/", 29) == 0) {
        int multiplier = kbo_clamp_intl_established_fa_multiplier(atoi(cmd + 29));
        if (kbo_set_intl_established_fa_multiplier(multiplier)) {
            append_logf("settings webview: international established FA multiplier=%d", multiplier);
        } else {
            append_logf("settings webview: failed to write international established FA multiplier=%d", multiplier);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "setleague/", 10) == 0) {
        uint32_t league_id = (uint32_t)strtoul(cmd + 10, NULL, 10);
        if (league_id != 0u) {
            g_kbo_hub_selected_league_id = league_id;
            g_kbo_hub_selected_team_id = 0;
            g_kbo_hub_open_dropdown = 0;
            kbo_hub_ensure_valid_selection();
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "setteam/", 8) == 0) {
        uint32_t team_id = (uint32_t)strtoul(cmd + 8, NULL, 10);
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "league/") == 0 || strcmp(cmd, "league") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 1 ? 0 : 1;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "team/") == 0 || strcmp(cmd, "team") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 2 ? 0 : 2;
        kbo_webview_navigate_current();
        return 1;
    }
    return 1;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args)
{
    (void)sender;
    KboWebViewNavHandler* handler = (KboWebViewNavHandler*)This;
    LPWSTR uri_w = NULL;
    if (args == NULL || FAILED(ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &uri_w)) || uri_w == NULL) {
        return S_OK;
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, NULL, 0, NULL, NULL);
    char uri[512] = {0};
    if (needed > 0) {
        WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, uri, sizeof(uri), NULL, NULL);
    }
    CoTaskMemFree(uri_w);
    if (kbo_webview_handle_command_uri(uri, handler->hwnd)) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    }
    return S_OK;
}

static const ICoreWebView2NavigationStartingEventHandlerVtbl g_kbo_webview_nav_vtbl = {
    kbo_webview_nav_qi,
    kbo_webview_nav_addref,
    kbo_webview_nav_release,
    kbo_webview_nav_invoke
};

static KboWebViewNavHandler g_kbo_webview_nav_handler = {
    { &g_kbo_webview_nav_vtbl },
    1,
    NULL
};

static EventRegistrationToken g_kbo_webview_nav_token = {0};

static HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewEnvHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewEnvHandler*)This)->ref);
    if (value < 1) { ((KboWebViewEnvHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewEnvHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewControllerHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewControllerHandler*)This)->ref);
    if (value < 1) { ((KboWebViewControllerHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewControllerHandler*)This)->ref;
}

static void kbo_webview_set_bounds(HWND hwnd)
{
    if (hwnd == NULL || g_kbo_webview_controller == NULL) {
        return;
    }
    RECT client;
    GetClientRect(hwnd, &client);
    RECT bounds = {0, 0, client.right, client.bottom};
    ICoreWebView2Controller_put_Bounds(g_kbo_webview_controller, bounds);
    ICoreWebView2Controller_put_IsVisible(g_kbo_webview_controller, TRUE);
}

static void kbo_webview_apply_ootp_like_settings(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }

    ICoreWebView2Settings* settings = NULL;
    if (FAILED(ICoreWebView2_get_Settings(g_kbo_webview, &settings)) || settings == NULL) {
        return;
    }

    ICoreWebView2Settings_put_IsStatusBarEnabled(settings, FALSE);
    ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, FALSE);
    ICoreWebView2Settings_put_AreDevToolsEnabled(settings, FALSE);
    ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
    ICoreWebView2Settings_Release(settings);
}

static HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result)
{
    KboWebViewControllerHandler* handler = (KboWebViewControllerHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        append_logf("WebView2 controller create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller = result;
    ICoreWebView2Controller_AddRef(g_kbo_webview_controller);
    ICoreWebView2Controller_get_CoreWebView2(g_kbo_webview_controller, &g_kbo_webview);
    if (g_kbo_webview != NULL) {
        kbo_webview_apply_ootp_like_settings();
        g_kbo_webview_nav_handler.hwnd = handler->hwnd;
        ICoreWebView2_add_NavigationStarting(g_kbo_webview, &g_kbo_webview_nav_handler.iface, &g_kbo_webview_nav_token);
        kbo_webview_navigate_current();
    }
    InterlockedExchange(&g_kbo_webview_ready, 1);
    kbo_webview_set_bounds(handler->hwnd);
    append_log_line("WebView2 F2 rights UI ready");
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_kbo_webview_controller_vtbl = {
    kbo_webview_controller_qi,
    kbo_webview_controller_addref,
    kbo_webview_controller_release,
    kbo_webview_controller_invoke
};

static KboWebViewControllerHandler g_kbo_webview_controller_handler = {
    { &g_kbo_webview_controller_vtbl },
    1,
    NULL
};

static HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Environment* result)
{
    KboWebViewEnvHandler* handler = (KboWebViewEnvHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        append_logf("WebView2 environment create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller_handler.hwnd = handler->hwnd;
    HRESULT hr = ICoreWebView2Environment_CreateCoreWebView2Controller(
        result,
        handler->hwnd,
        &g_kbo_webview_controller_handler.iface);
    if (FAILED(hr)) {
        append_logf("WebView2 CreateCoreWebView2Controller failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    }
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_kbo_webview_env_vtbl = {
    kbo_webview_env_qi,
    kbo_webview_env_addref,
    kbo_webview_env_release,
    kbo_webview_env_invoke
};

static KboWebViewEnvHandler g_kbo_webview_env_handler = {
    { &g_kbo_webview_env_vtbl },
    1,
    NULL
};

static void kbo_start_webview_rights_ui(HWND hwnd)
{
    if (hwnd == NULL || InterlockedCompareExchange(&g_kbo_webview_starting, 1, 0) != 0) {
        kbo_webview_set_bounds(hwnd);
        return;
    }

    HMODULE self_module = NULL;
    char loader_path[MAX_PATH] = {0};
    HMODULE loader = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_start_webview_rights_ui, &self_module)
            && GetModuleFileNameA(self_module, loader_path, sizeof(loader_path)) > 0) {
        char* slash = strrchr(loader_path, '\\');
        if (slash != NULL) {
            slash[1] = '\0';
            strncat(loader_path, "WebView2Loader.dll", sizeof(loader_path) - strlen(loader_path) - 1);
            loader = LoadLibraryA(loader_path);
        }
    }
    if (loader == NULL) {
        loader = LoadLibraryA("WebView2Loader.dll");
    }
    if (loader == NULL) {
        append_logf("WebView2Loader.dll load failed error=%lu", GetLastError());
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    KboCreateCoreWebView2EnvironmentWithOptionsFn create_env =
        (KboCreateCoreWebView2EnvironmentWithOptionsFn)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (create_env == NULL) {
        append_log_line("WebView2 CreateCoreWebView2EnvironmentWithOptions missing");
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    WCHAR user_data[MAX_PATH] = {0};
    WCHAR local[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local, (DWORD)(sizeof(local) / sizeof(local[0])));
    if (got > 0 && got < (DWORD)(sizeof(local) / sizeof(local[0]))) {
        swprintf(user_data, sizeof(user_data) / sizeof(user_data[0]), L"%ls\\OOTP-KBO\\WebView2", local);
        CreateDirectoryW(user_data, NULL);
    }

    g_kbo_webview_env_handler.hwnd = hwnd;
    HRESULT hr = create_env(NULL, user_data[0] != L'\0' ? user_data : NULL, NULL, &g_kbo_webview_env_handler.iface);
    if (FAILED(hr)) {
        append_logf("WebView2 environment start failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    } else {
        append_log_line("WebView2 F2 rights UI starting");
    }
}
