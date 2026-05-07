static int kbo_hub_read_style_int(const char* path, const char* key, int default_value)
{
    if (path == NULL || key == NULL) {
        return default_value;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return default_value;
    }

    char line[256];
    int expect_value = 0;
    int result = default_value;
    while (fgets(line, sizeof(line), file) != NULL) {
        kbo_hub_trim_ascii(line);
        if (line[0] == '\0') {
            continue;
        }
        if (expect_value) {
            result = atoi(line);
            break;
        }
        if (strcmp(line, key) == 0) {
            expect_value = 1;
        }
    }

    fclose(file);
    return result;
}

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

    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27");
}

static void kbo_hub_ootp_install_path(const char* relative_path, char* out, size_t out_size)
{
    if (relative_path == NULL || out == NULL || out_size == 0) {
        return;
    }

    char install_dir[MAX_PATH] = {0};
    kbo_hub_copy_ootp_install_dir(install_dir, sizeof(install_dir));
    snprintf(out, out_size, "%s\\%s", install_dir, relative_path);
}

static void kbo_hub_load_skin_metrics(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_metrics_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH] = {0};

    kbo_hub_ootp_install_path(
        "data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        path,
        sizeof(path));
    g_kbo_hub_skin_article_gap_x = kbo_hub_read_style_int(
        path,
        "border_left_gap",
        g_kbo_hub_skin_article_gap_x);
    g_kbo_hub_skin_article_gap_y = kbo_hub_read_style_int(
        path,
        "border_top_gap",
        g_kbo_hub_skin_article_gap_y);
    g_kbo_hub_skin_article_font_px = kbo_hub_read_style_int(
        path,
        "font_x_sz",
        g_kbo_hub_skin_article_font_px);

    kbo_hub_ootp_install_path(
        "data\\skins\\ootp dark\\style_sets\\article\\button.ss",
        path,
        sizeof(path));
    g_kbo_hub_skin_button_font_px = kbo_hub_read_style_int(
        path,
        "font_x_sz",
        g_kbo_hub_skin_button_font_px);

    kbo_hub_ootp_install_path(
        "data\\skins\\ootp dark\\style_sets\\table_scrollbar\\scrollbar.ss",
        path,
        sizeof(path));
    g_kbo_hub_skin_scrollbar_width = kbo_hub_read_style_int(
        path,
        "scrollbar_button_width",
        g_kbo_hub_skin_scrollbar_width);
}

static void kbo_hub_skin_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\skins\\ootp dark\\images\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

static void kbo_hub_skin_scrollbar_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\skins\\ootp dark\\style_sets\\table_scrollbar\\images\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

static void kbo_hub_skin_button_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\skins\\ootp dark\\style_sets\\buttons\\images\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

static void kbo_hub_nation_flag_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\database\\nation_flags\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}

static void kbo_hub_local_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Users\\user\\OOTP27-KBO-Launcher\\assets\\icons\\%s",
        file_name);
}

static void kbo_hub_font_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Users\\user\\OOTP27-KBO-Launcher\\assets\\fonts\\%s",
        file_name);
}

static void kbo_hub_logo_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    char relative[MAX_PATH] = {0};
    snprintf(relative, sizeof(relative), "data\\logos\\%s", file_name);
    kbo_hub_ootp_install_path(relative, out, out_size);
}
