#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../../../core/dates/core_text_date.h"
#include "ui_language.h"

static int g_kbo_hub_language = KBO_HUB_LANG_KO;

int kbo_hub_language(void)
{
    return g_kbo_hub_language;
}

void kbo_hub_set_language(int language)
{
    g_kbo_hub_language = language == KBO_HUB_LANG_EN ? KBO_HUB_LANG_EN : KBO_HUB_LANG_KO;
}

const char* kbo_hub_text(const char* ko, const char* en)
{
    return g_kbo_hub_language == KBO_HUB_LANG_EN ? en : ko;
}

static void kbo_hub_language_file_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\hub_language.txt", local_app_data);
}

void kbo_hub_load_language_setting(void)
{
    g_kbo_hub_language = KBO_HUB_LANG_KO;

    char path[MAX_PATH] = {0};
    kbo_hub_language_file_path(path, sizeof(path));
    if (path[0] == '\0') {
        return;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char buffer[16] = {0};
    DWORD read = 0;
    ReadFile(file, buffer, sizeof(buffer) - 1, &read, NULL);
    CloseHandle(file);

    if (ascii_equals_ignore_case(buffer, "en") || ascii_equals_ignore_case(buffer, "english")) {
        g_kbo_hub_language = KBO_HUB_LANG_EN;
    }
}

void kbo_hub_save_language_setting(void)
{
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s\\OOTP-KBO", local_app_data);
    CreateDirectoryA(dir, NULL);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\hub_language.txt", dir);

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const char* value = g_kbo_hub_language == KBO_HUB_LANG_EN ? "en\n" : "ko\n";
    DWORD wrote = 0;
    WriteFile(file, value, (DWORD)strlen(value), &wrote, NULL);
    CloseHandle(file);
}
