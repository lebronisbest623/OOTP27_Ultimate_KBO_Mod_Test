#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../../../core/logging/core_log.h"
#include "ui_asset_paths.h"

static void kbo_hub_copy_ootp_install_dir(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char exe_path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len > 0 && len < sizeof(exe_path)) {
        char* slash = strrchr(exe_path, '\\');
        if (slash != NULL) {
            *slash = '\0';
            char skin_dir[MAX_PATH] = {0};
            snprintf(skin_dir, sizeof(skin_dir), "%s\\data\\skins\\ootp dark", exe_path);
            DWORD attrs = GetFileAttributesA(skin_dir);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                snprintf(out, out_size, "%s", exe_path);
                return;
            }
        }
    }

    kbo_log_runtime_line("KBO F2 hub OOTP install dir unavailable; OOTP skin assets disabled");
}

void kbo_hub_ootp_install_path(const char* relative_path, char* out, size_t out_size)
{
    if (relative_path == NULL || out == NULL || out_size == 0) {
        return;
    }

    char install_dir[MAX_PATH] = {0};
    kbo_hub_copy_ootp_install_dir(install_dir, sizeof(install_dir));
    if (install_dir[0] == '\0') {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, "%s\\%s", install_dir, relative_path);
}

static int kbo_hub_copy_self_module_dir(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    HMODULE self_module = NULL;
    char path[MAX_PATH] = {0};
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_hub_copy_self_module_dir,
            &self_module)) {
        return 0;
    }
    DWORD len = GetModuleFileNameA(self_module, path, sizeof(path));
    if (len == 0 || len >= sizeof(path)) {
        return 0;
    }

    char* slash = strrchr(path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';
    snprintf(out, out_size, "%s", path);
    return 1;
}

static void kbo_hub_local_asset_path_with_type(const char* type, const char* file_name, char* out, size_t out_size)
{
    if (type == NULL || file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char module_dir[MAX_PATH] = {0};
    if (kbo_hub_copy_self_module_dir(module_dir, sizeof(module_dir))) {
        snprintf(out, out_size, "%s\\assets\\%s\\%s", module_dir, type, file_name);
    }
}

void kbo_hub_skin_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\skins\\ootp dark\\images\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

void kbo_hub_skin_scrollbar_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\skins\\ootp dark\\style_sets\\table_scrollbar\\images\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

void kbo_hub_skin_button_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\skins\\ootp dark\\style_sets\\buttons\\images\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

void kbo_hub_nation_flag_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\database\\nation_flags\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

void kbo_hub_local_asset_path(const char* file_name, char* out, size_t out_size)
{
    kbo_hub_local_asset_path_with_type("icons", file_name, out, out_size);
}

void kbo_hub_font_asset_path(const char* file_name, char* out, size_t out_size)
{
    kbo_hub_local_asset_path_with_type("fonts", file_name, out, out_size);
}

void kbo_hub_logo_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\logos\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}
