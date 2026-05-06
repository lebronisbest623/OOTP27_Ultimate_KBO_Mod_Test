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
static void kbo_hub_load_skin_metrics(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_metrics_loaded, 1, 0) != 0) {
        return;
    }

    g_kbo_hub_skin_article_gap_x = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        "border_left_gap",
        g_kbo_hub_skin_article_gap_x);
    g_kbo_hub_skin_article_gap_y = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        "border_top_gap",
        g_kbo_hub_skin_article_gap_y);
    g_kbo_hub_skin_article_font_px = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        "font_x_sz",
        g_kbo_hub_skin_article_font_px);
    g_kbo_hub_skin_button_font_px = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\button.ss",
        "font_x_sz",
        g_kbo_hub_skin_button_font_px);
    g_kbo_hub_skin_scrollbar_width = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\table_scrollbar\\scrollbar.ss",
        "scrollbar_button_width",
        g_kbo_hub_skin_scrollbar_width);
}

static void kbo_hub_skin_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\images\\%s",
        file_name);
}

static void kbo_hub_skin_scrollbar_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\table_scrollbar\\images\\%s",
        file_name);
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

static void kbo_hub_logo_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\logos\\%s",
        file_name);
}
