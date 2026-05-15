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
    char plus_up[2048] = {0};
    char plus_over[2048] = {0};
    char plus_down[2048] = {0};

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
    kbo_webview_copy_button_skin_src("list_buttons_plus_up.png", plus_up, sizeof(plus_up));
    kbo_webview_copy_button_skin_src("list_buttons_plus_over.png", plus_over, sizeof(plus_over));
    kbo_webview_copy_button_skin_src("list_buttons_plus_down.png", plus_down, sizeof(plus_down));

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
        "--ootp-btn-minus-up:url('%s');--ootp-btn-minus-over:url('%s');--ootp-btn-minus-down:url('%s');"
        "--ootp-btn-plus-up:url('%s');--ootp-btn-plus-over:url('%s');--ootp-btn-plus-down:url('%s')}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar:vertical{width:var(--ootp-sb-width)!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-track:vertical{background-color:#101010!important;background-image:var(--ootp-sb-bar-top),var(--ootp-sb-bar-bottom),var(--ootp-sb-bar-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-thumb:vertical{min-height:42px;background-color:#2a2a2a!important;background-image:var(--ootp-sb-slider-up-top),var(--ootp-sb-slider-up-bottom),var(--ootp-sb-slider-up-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-thumb:vertical:hover{background-image:var(--ootp-sb-slider-over-top),var(--ootp-sb-slider-over-bottom),var(--ootp-sb-slider-over-mid)!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-thumb:vertical:active{background-image:var(--ootp-sb-slider-down-top),var(--ootp-sb-slider-down-bottom),var(--ootp-sb-slider-down-mid)!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:decrement{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-less-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:increment{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-more-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:decrement:hover{background-image:var(--ootp-sb-less-over)!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:increment:hover{background-image:var(--ootp-sb-more-over)!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:decrement:active{background-image:var(--ootp-sb-less-down)!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:increment:active{background-image:var(--ootp-sb-more-down)!important}",
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
        minus_down,
        plus_up,
        plus_over,
        plus_down);
    kbo_window_text_appendf(
        &css,
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar:vertical{width:%dpx!important;background:transparent!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar:horizontal{display:none!important;height:0!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-track:vertical{background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-thumb:vertical{min-height:42px!important;background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-thumb:vertical:hover{background-image:url('%s'),url('%s'),url('%s')!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-thumb:vertical:active{background-image:url('%s'),url('%s'),url('%s')!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:decrement{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:increment{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:decrement:hover{background-image:url('%s')!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:increment:hover{background-image:url('%s')!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:decrement:active{background-image:url('%s')!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-button:vertical:increment:active{background-image:url('%s')!important}"
        ":is(.dropdown,.faFilterMenu,.ootpChoiceMenu,.content,.rights,.settingsGrid,.settingsCard,.tablewrap,.card)::-webkit-scrollbar-corner{background:transparent!important}",
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
