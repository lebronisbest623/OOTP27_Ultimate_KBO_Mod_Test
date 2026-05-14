#include "../ui_mod_info_views_internal.h"

#include "../../../../../core/core_flags/json/json_bool_parser.h"
#include "../../../../../core/files/save_paths/core_save_paths.h"

#define KBO_MOD_CREDITS_FILE "mod_credits.json"
#define KBO_MOD_CREDITS_TESTER_MAX 128

static char* kbo_mod_credits_read_json_file(DWORD* out_size)
{
    if (out_size == NULL) {
        return NULL;
    }
    *out_size = 0u;

    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_MOD_CREDITS_FILE, path, sizeof(path))) {
        return NULL;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return NULL;
    }

    char* json = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (json == NULL) {
        CloseHandle(file);
        return NULL;
    }

    DWORD read = 0u;
    if (!ReadFile(file, json, size, &read, NULL) || read != size) {
        HeapFree(GetProcessHeap(), 0, json);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);

    json[size] = '\0';
    *out_size = size;
    return json;
}

static int kbo_webview_append_mod_tester_json_array(KboWindowTextBuffer* buffer, const char* start, const char* end)
{
    int appended = 0;
    const char* p = kbo_json_skip_ws(start, end);
    if (p == NULL || p >= end || *p != '[') {
        return 0;
    }

    for (++p; p < end; ++p) {
        p = kbo_json_skip_ws(p, end);
        if (p == NULL || p >= end || *p == ']') {
            break;
        }
        if (*p != '"') {
            continue;
        }

        const char* string_end = kbo_json_find_string_end(p, end);
        if (string_end == NULL || string_end >= end) {
            break;
        }

        char tester[KBO_MOD_CREDITS_TESTER_MAX] = {0};
        if (kbo_json_string_value_at(p, string_end + 1, tester, sizeof(tester)) && tester[0] != '\0') {
            if (appended > 0) {
                kbo_window_text_appendf(buffer, "</p><p>");
            }
            kbo_html_append_escaped(buffer, tester);
            appended += 1;
        }
        p = string_end;
    }

    return appended;
}

static int kbo_webview_append_mod_tester_credits_from_json(KboWindowTextBuffer* buffer)
{
    DWORD json_size = 0u;
    char* json = kbo_mod_credits_read_json_file(&json_size);
    if (json == NULL) {
        return 0;
    }

    const char* start = NULL;
    const char* end = NULL;
    int appended = kbo_find_json_value_span(json, json_size, "testers", &start, &end)
        ? kbo_webview_append_mod_tester_json_array(buffer, start, end)
        : 0;

    HeapFree(GetProcessHeap(), 0, json);
    return appended;
}

void kbo_webview_append_mod_tester_credits(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (kbo_webview_append_mod_tester_credits_from_json(buffer) > 0) {
        return;
    }

    kbo_html_append_escaped(buffer, "michaelisgoat");
}
