#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdio.h>

#include "../assets/paths/ui_asset_paths.h"
#include "../assets/paths/ui_image_sources.h"
#include "ui_scrollbar_skin_css.h"
#include "../text/buffer/ui_text_buffer.h"

static void kbo_webview_copy_scrollbar_skin_src(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char path[MAX_PATH] = {0};
    kbo_hub_skin_scrollbar_image_path(file_name, path, sizeof(path));
    kbo_webview_copy_image_src(path, out, out_size);
}

static void kbo_webview_copy_button_skin_src(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char path[MAX_PATH] = {0};
    kbo_hub_skin_button_image_path(file_name, path, sizeof(path));
    kbo_webview_copy_image_src(path, out, out_size);
}

void kbo_webview_build_scrollbar_skin_css(char* out, size_t out_size, int scrollbar_width)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char bar_top[2048] = {0};
    char bar_mid[2048] = {0};
    char bar_bottom[2048] = {0};
    char less_up[2048] = {0};
    char less_over[2048] = {0};
    char less_down[2048] = {0};
    char more_up[2048] = {0};
    char more_over[2048] = {0};
    char more_down[2048] = {0};
    char slider_up_top[2048] = {0};
    char slider_up_mid[2048] = {0};
    char slider_up_bottom[2048] = {0};
    char slider_over_top[2048] = {0};
    char slider_over_mid[2048] = {0};
    char slider_over_bottom[2048] = {0};
    char slider_down_top[2048] = {0};
    char slider_down_mid[2048] = {0};
    char slider_down_bottom[2048] = {0};
    char minus_up[2048] = {0};
    char minus_over[2048] = {0};
    char minus_down[2048] = {0};

    kbo_webview_copy_scrollbar_skin_src("sb_bar_top.png", bar_top, sizeof(bar_top));
    kbo_webview_copy_scrollbar_skin_src("sb_bar_mid.png", bar_mid, sizeof(bar_mid));
    kbo_webview_copy_scrollbar_skin_src("sb_bar_bottom.png", bar_bottom, sizeof(bar_bottom));
    kbo_webview_copy_scrollbar_skin_src("sb_less_up.png", less_up, sizeof(less_up));
    kbo_webview_copy_scrollbar_skin_src("sb_less_over.png", less_over, sizeof(less_over));
    kbo_webview_copy_scrollbar_skin_src("sb_less_down.png", less_down, sizeof(less_down));
    kbo_webview_copy_scrollbar_skin_src("sb_more_up.png", more_up, sizeof(more_up));
    kbo_webview_copy_scrollbar_skin_src("sb_more_over.png", more_over, sizeof(more_over));
    kbo_webview_copy_scrollbar_skin_src("sb_more_down.png", more_down, sizeof(more_down));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_up_top.png", slider_up_top, sizeof(slider_up_top));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_up_mid.png", slider_up_mid, sizeof(slider_up_mid));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_up_bottom.png", slider_up_bottom, sizeof(slider_up_bottom));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_over_top.png", slider_over_top, sizeof(slider_over_top));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_over_mid.png", slider_over_mid, sizeof(slider_over_mid));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_over_bottom.png", slider_over_bottom, sizeof(slider_over_bottom));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_down_top.png", slider_down_top, sizeof(slider_down_top));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_down_mid.png", slider_down_mid, sizeof(slider_down_mid));
    kbo_webview_copy_scrollbar_skin_src("sb_slider_down_bottom.png", slider_down_bottom, sizeof(slider_down_bottom));
    kbo_webview_copy_button_skin_src("list_buttons_minus_up.png", minus_up, sizeof(minus_up));
    kbo_webview_copy_button_skin_src("list_buttons_minus_over.png", minus_over, sizeof(minus_over));
    kbo_webview_copy_button_skin_src("list_buttons_minus_down.png", minus_down, sizeof(minus_down));

    if (scrollbar_width < 12) {
        scrollbar_width = 20;
    }

    KboWindowTextBuffer css;
    css.data = out;
    css.capacity = out_size;
    css.length = 0;
    kbo_window_text_appendf(
        &css,
        ":root{--ootp-sb-width:%dpx;--ootp-sb-bar-top:url('%s');--ootp-sb-bar-mid:url('%s');--ootp-sb-bar-bottom:url('%s');"
        "--ootp-sb-less-up:url('%s');--ootp-sb-less-over:url('%s');--ootp-sb-less-down:url('%s');"
        "--ootp-sb-more-up:url('%s');--ootp-sb-more-over:url('%s');--ootp-sb-more-down:url('%s');"
        "--ootp-sb-slider-up-top:url('%s');--ootp-sb-slider-up-mid:url('%s');--ootp-sb-slider-up-bottom:url('%s');"
        "--ootp-sb-slider-over-top:url('%s');--ootp-sb-slider-over-mid:url('%s');--ootp-sb-slider-over-bottom:url('%s');"
        "--ootp-sb-slider-down-top:url('%s');--ootp-sb-slider-down-mid:url('%s');--ootp-sb-slider-down-bottom:url('%s');"
        "--ootp-btn-minus-up:url('%s');--ootp-btn-minus-over:url('%s');--ootp-btn-minus-down:url('%s')}"
        ".dropdown::-webkit-scrollbar:vertical,.ootpChoiceMenu::-webkit-scrollbar:vertical,.content::-webkit-scrollbar:vertical,.rights::-webkit-scrollbar:vertical,.settingsGrid::-webkit-scrollbar:vertical,.settingsCard::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:var(--ootp-sb-width)!important}"
        ".dropdown::-webkit-scrollbar-track:vertical,.ootpChoiceMenu::-webkit-scrollbar-track:vertical,.content::-webkit-scrollbar-track:vertical,.rights::-webkit-scrollbar-track:vertical,.settingsGrid::-webkit-scrollbar-track:vertical,.settingsCard::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background-color:#101010!important;background-image:var(--ootp-sb-bar-top),var(--ootp-sb-bar-bottom),var(--ootp-sb-bar-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical,.ootpChoiceMenu::-webkit-scrollbar-thumb:vertical,.content::-webkit-scrollbar-thumb:vertical,.rights::-webkit-scrollbar-thumb:vertical,.settingsGrid::-webkit-scrollbar-thumb:vertical,.settingsCard::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px;background-color:#2a2a2a!important;background-image:var(--ootp-sb-slider-up-top),var(--ootp-sb-slider-up-bottom),var(--ootp-sb-slider-up-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:hover,.ootpChoiceMenu::-webkit-scrollbar-thumb:vertical:hover,.content::-webkit-scrollbar-thumb:vertical:hover,.rights::-webkit-scrollbar-thumb:vertical:hover,.settingsGrid::-webkit-scrollbar-thumb:vertical:hover,.settingsCard::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:var(--ootp-sb-slider-over-top),var(--ootp-sb-slider-over-bottom),var(--ootp-sb-slider-over-mid)!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:active,.ootpChoiceMenu::-webkit-scrollbar-thumb:vertical:active,.content::-webkit-scrollbar-thumb:vertical:active,.rights::-webkit-scrollbar-thumb:vertical:active,.settingsGrid::-webkit-scrollbar-thumb:vertical:active,.settingsCard::-webkit-scrollbar-thumb:vertical:active,.tablewrap::-webkit-scrollbar-thumb:vertical:active,.card::-webkit-scrollbar-thumb:vertical:active{background-image:var(--ootp-sb-slider-down-top),var(--ootp-sb-slider-down-bottom),var(--ootp-sb-slider-down-mid)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:decrement,.content::-webkit-scrollbar-button:vertical:decrement,.rights::-webkit-scrollbar-button:vertical:decrement,.settingsGrid::-webkit-scrollbar-button:vertical:decrement,.settingsCard::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-less-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:increment,.content::-webkit-scrollbar-button:vertical:increment,.rights::-webkit-scrollbar-button:vertical:increment,.settingsGrid::-webkit-scrollbar-button:vertical:increment,.settingsCard::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-more-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:hover,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:decrement:hover,.content::-webkit-scrollbar-button:vertical:decrement:hover,.rights::-webkit-scrollbar-button:vertical:decrement:hover,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:hover,.settingsCard::-webkit-scrollbar-button:vertical:decrement:hover,.tablewrap::-webkit-scrollbar-button:vertical:decrement:hover,.card::-webkit-scrollbar-button:vertical:decrement:hover{background-image:var(--ootp-sb-less-over)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:hover,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:increment:hover,.content::-webkit-scrollbar-button:vertical:increment:hover,.rights::-webkit-scrollbar-button:vertical:increment:hover,.settingsGrid::-webkit-scrollbar-button:vertical:increment:hover,.settingsCard::-webkit-scrollbar-button:vertical:increment:hover,.tablewrap::-webkit-scrollbar-button:vertical:increment:hover,.card::-webkit-scrollbar-button:vertical:increment:hover{background-image:var(--ootp-sb-more-over)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:active,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:decrement:active,.content::-webkit-scrollbar-button:vertical:decrement:active,.rights::-webkit-scrollbar-button:vertical:decrement:active,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:active,.settingsCard::-webkit-scrollbar-button:vertical:decrement:active,.tablewrap::-webkit-scrollbar-button:vertical:decrement:active,.card::-webkit-scrollbar-button:vertical:decrement:active{background-image:var(--ootp-sb-less-down)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:active,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:increment:active,.content::-webkit-scrollbar-button:vertical:increment:active,.rights::-webkit-scrollbar-button:vertical:increment:active,.settingsGrid::-webkit-scrollbar-button:vertical:increment:active,.settingsCard::-webkit-scrollbar-button:vertical:increment:active,.tablewrap::-webkit-scrollbar-button:vertical:increment:active,.card::-webkit-scrollbar-button:vertical:increment:active{background-image:var(--ootp-sb-more-down)!important}",
        scrollbar_width,
        bar_top,
        bar_mid,
        bar_bottom,
        less_up,
        less_over,
        less_down,
        more_up,
        more_over,
        more_down,
        slider_up_top,
        slider_up_mid,
        slider_up_bottom,
        slider_over_top,
        slider_over_mid,
        slider_over_bottom,
        slider_down_top,
        slider_down_mid,
        slider_down_bottom,
        minus_up,
        minus_over,
        minus_down);
    kbo_window_text_appendf(
        &css,
        ".dropdown::-webkit-scrollbar:vertical,.ootpChoiceMenu::-webkit-scrollbar:vertical,.content::-webkit-scrollbar:vertical,.rights::-webkit-scrollbar:vertical,.settingsGrid::-webkit-scrollbar:vertical,.settingsCard::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:%dpx!important;background:transparent!important}"
        ".dropdown::-webkit-scrollbar:horizontal,.ootpChoiceMenu::-webkit-scrollbar:horizontal,.content::-webkit-scrollbar:horizontal,.rights::-webkit-scrollbar:horizontal,.settingsGrid::-webkit-scrollbar:horizontal,.settingsCard::-webkit-scrollbar:horizontal,.tablewrap::-webkit-scrollbar:horizontal,.card::-webkit-scrollbar:horizontal{display:none!important;height:0!important}"
        ".dropdown::-webkit-scrollbar-track:vertical,.ootpChoiceMenu::-webkit-scrollbar-track:vertical,.content::-webkit-scrollbar-track:vertical,.rights::-webkit-scrollbar-track:vertical,.settingsGrid::-webkit-scrollbar-track:vertical,.settingsCard::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical,.ootpChoiceMenu::-webkit-scrollbar-thumb:vertical,.content::-webkit-scrollbar-thumb:vertical,.rights::-webkit-scrollbar-thumb:vertical,.settingsGrid::-webkit-scrollbar-thumb:vertical,.settingsCard::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px!important;background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:hover,.ootpChoiceMenu::-webkit-scrollbar-thumb:vertical:hover,.content::-webkit-scrollbar-thumb:vertical:hover,.rights::-webkit-scrollbar-thumb:vertical:hover,.settingsGrid::-webkit-scrollbar-thumb:vertical:hover,.settingsCard::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:url('%s'),url('%s'),url('%s')!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:active,.ootpChoiceMenu::-webkit-scrollbar-thumb:vertical:active,.content::-webkit-scrollbar-thumb:vertical:active,.rights::-webkit-scrollbar-thumb:vertical:active,.settingsGrid::-webkit-scrollbar-thumb:vertical:active,.settingsCard::-webkit-scrollbar-thumb:vertical:active,.tablewrap::-webkit-scrollbar-thumb:vertical:active,.card::-webkit-scrollbar-thumb:vertical:active{background-image:url('%s'),url('%s'),url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:decrement,.content::-webkit-scrollbar-button:vertical:decrement,.rights::-webkit-scrollbar-button:vertical:decrement,.settingsGrid::-webkit-scrollbar-button:vertical:decrement,.settingsCard::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:increment,.content::-webkit-scrollbar-button:vertical:increment,.rights::-webkit-scrollbar-button:vertical:increment,.settingsGrid::-webkit-scrollbar-button:vertical:increment,.settingsCard::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:hover,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:decrement:hover,.content::-webkit-scrollbar-button:vertical:decrement:hover,.rights::-webkit-scrollbar-button:vertical:decrement:hover,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:hover,.settingsCard::-webkit-scrollbar-button:vertical:decrement:hover,.tablewrap::-webkit-scrollbar-button:vertical:decrement:hover,.card::-webkit-scrollbar-button:vertical:decrement:hover{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:hover,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:increment:hover,.content::-webkit-scrollbar-button:vertical:increment:hover,.rights::-webkit-scrollbar-button:vertical:increment:hover,.settingsGrid::-webkit-scrollbar-button:vertical:increment:hover,.settingsCard::-webkit-scrollbar-button:vertical:increment:hover,.tablewrap::-webkit-scrollbar-button:vertical:increment:hover,.card::-webkit-scrollbar-button:vertical:increment:hover{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:active,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:decrement:active,.content::-webkit-scrollbar-button:vertical:decrement:active,.rights::-webkit-scrollbar-button:vertical:decrement:active,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:active,.settingsCard::-webkit-scrollbar-button:vertical:decrement:active,.tablewrap::-webkit-scrollbar-button:vertical:decrement:active,.card::-webkit-scrollbar-button:vertical:decrement:active{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:active,.ootpChoiceMenu::-webkit-scrollbar-button:vertical:increment:active,.content::-webkit-scrollbar-button:vertical:increment:active,.rights::-webkit-scrollbar-button:vertical:increment:active,.settingsGrid::-webkit-scrollbar-button:vertical:increment:active,.settingsCard::-webkit-scrollbar-button:vertical:increment:active,.tablewrap::-webkit-scrollbar-button:vertical:increment:active,.card::-webkit-scrollbar-button:vertical:increment:active{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-corner,.ootpChoiceMenu::-webkit-scrollbar-corner,.content::-webkit-scrollbar-corner,.rights::-webkit-scrollbar-corner,.settingsGrid::-webkit-scrollbar-corner,.settingsCard::-webkit-scrollbar-corner,.tablewrap::-webkit-scrollbar-corner,.card::-webkit-scrollbar-corner{background:transparent!important}",
        scrollbar_width,
        bar_top,
        bar_bottom,
        bar_mid,
        slider_up_top,
        slider_up_bottom,
        slider_up_mid,
        slider_over_top,
        slider_over_bottom,
        slider_over_mid,
        slider_down_top,
        slider_down_bottom,
        slider_down_mid,
        scrollbar_width,
        less_up,
        scrollbar_width,
        more_up,
        less_over,
        more_over,
        less_down,
        more_down);
}
