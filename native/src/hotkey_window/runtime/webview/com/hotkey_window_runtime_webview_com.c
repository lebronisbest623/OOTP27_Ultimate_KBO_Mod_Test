#include "../hotkey_window_webview_internal.h"

HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
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

static ICoreWebView2NavigationStartingEventHandlerVtbl g_kbo_webview_nav_vtbl = {
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

HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv)
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

ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewEnvHandler*)This)->ref);
}

ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewEnvHandler*)This)->ref);
    if (value < 1) { ((KboWebViewEnvHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewEnvHandler*)This)->ref;
}

HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv)
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

ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewControllerHandler*)This)->ref);
}

ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewControllerHandler*)This)->ref);
    if (value < 1) { ((KboWebViewControllerHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewControllerHandler*)This)->ref;
}

void kbo_webview_set_bounds(HWND hwnd)
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

void kbo_webview_apply_ootp_like_settings(void)
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

HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
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

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_kbo_webview_controller_vtbl = {
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

HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
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

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_kbo_webview_env_vtbl = {
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

void kbo_start_webview_rights_ui(HWND hwnd)
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

    union {
        FARPROC proc;
        KboCreateCoreWebView2EnvironmentWithOptionsFn fn;
    } create_env_lookup;
    create_env_lookup.proc = GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    KboCreateCoreWebView2EnvironmentWithOptionsFn create_env = create_env_lookup.fn;
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

