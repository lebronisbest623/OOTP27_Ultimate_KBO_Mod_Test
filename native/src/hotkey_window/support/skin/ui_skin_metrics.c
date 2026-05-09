#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../state/text/state_text_utils.h"
#include "../assets/paths/ui_asset_paths.h"
#include "ui_skin_metrics.h"

static LONG g_kbo_hub_skin_metrics_loaded = 0;
static int g_kbo_hub_skin_article_gap_x = 16;
static int g_kbo_hub_skin_article_gap_y = 8;
static int g_kbo_hub_skin_article_font_px = 16;
static int g_kbo_hub_skin_button_font_px = 16;
static int g_kbo_hub_skin_scrollbar_width = 20;

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

void kbo_hub_load_skin_metrics(void)
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
int kbo_hub_skin_article_gap_x(void) { return g_kbo_hub_skin_article_gap_x; }
int kbo_hub_skin_article_gap_y(void) { return g_kbo_hub_skin_article_gap_y; }
int kbo_hub_skin_article_font_px(void) { return g_kbo_hub_skin_article_font_px; }
int kbo_hub_skin_button_font_px(void) { return g_kbo_hub_skin_button_font_px; }
int kbo_hub_skin_scrollbar_width(void) { return g_kbo_hub_skin_scrollbar_width; }