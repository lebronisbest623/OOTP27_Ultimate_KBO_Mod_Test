#include "ui_html_render/view_military.inc"
#include "ui_html_render/view_foreign_rights.inc"
#include "ui_html_render/view_asian_quota.inc"
#include "ui_html_render/view_asian_games.inc"
#include "ui_html_render/view_fa_cases.inc"
#include "ui_html_render/view_mod_info.inc"
#include "ui_html_render/view_settings.inc"
#include "ui_html_render/view_fallback.inc"
#include "ui_html_render/view_router.inc"

static void kbo_webview_append_js_string(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "'");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch == '\\' || ch == '\'') {
                kbo_window_text_appendf(buffer, "\\%c", ch);
            } else if (ch == '\n') {
                kbo_window_text_appendf(buffer, "\\n");
            } else if (ch == '\r') {
                kbo_window_text_appendf(buffer, "\\r");
            } else if (ch < 0x20u) {
                kbo_window_text_appendf(buffer, "\\x%02x", (unsigned int)ch);
            } else {
                kbo_window_text_appendf(buffer, "%c", ch);
            }
        }
    }
    kbo_window_text_appendf(buffer, "'");
}

static void kbo_webview_append_main_tabs(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    static const int main_views[] = {
        KBO_HUB_VIEW_MOD_INFO,
        KBO_HUB_VIEW_MILITARY,
        KBO_HUB_VIEW_ASIAN_QUOTA,
        KBO_HUB_VIEW_ASIAN_GAMES,
        KBO_HUB_VIEW_FA_CASES,
        KBO_HUB_VIEW_SETTINGS
    };
    for (size_t i = 0; i < sizeof(main_views) / sizeof(main_views[0]); i++) {
        int view = main_views[i];
        kbo_window_text_appendf(buffer, "<a class='mainTab %s' href='kbo://view/%d'>",
            view == g_kbo_hub_selected_view ? "active" : "", view);
        kbo_html_append_escaped(buffer, kbo_hub_nav_label(view));
        kbo_window_text_appendf(buffer, "</a>");
    }
}

static void kbo_webview_append_sub_tabs(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO) {
        for (int i = 0; i < KBO_HUB_MOD_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://mod/%d'>",
                i == g_kbo_hub_selected_mod_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_mod_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        for (int i = 0; i < KBO_HUB_MILITARY_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://military/%d'>",
                i == g_kbo_hub_selected_military_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_military_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        for (int i = 0; i < KBO_HUB_FOREIGN_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://foreign/%d'>",
                i == g_kbo_hub_selected_foreign_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_foreign_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        for (int i = 0; i < KBO_HUB_AGAMES_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://agames/%d'>",
                i == g_kbo_hub_selected_agames_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_agames_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES) {
        for (int i = 0; i < KBO_HUB_FA_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://fa/%d'>",
                i == g_kbo_hub_selected_fa_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_fa_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    }
}

static int kbo_webview_current_view_has_sub_tabs(void)
{
    return g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS;
}

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

static void kbo_webview_build_scrollbar_skin_css(char* out, size_t out_size)
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

    int scrollbar_width = g_kbo_hub_skin_scrollbar_width;
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
        ".dropdown::-webkit-scrollbar:vertical,.content::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:var(--ootp-sb-width)!important}"
        ".dropdown::-webkit-scrollbar-track:vertical,.content::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background-color:#101010!important;background-image:var(--ootp-sb-bar-top),var(--ootp-sb-bar-bottom),var(--ootp-sb-bar-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical,.content::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px;background-color:#2a2a2a!important;background-image:var(--ootp-sb-slider-up-top),var(--ootp-sb-slider-up-bottom),var(--ootp-sb-slider-up-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:hover,.content::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:var(--ootp-sb-slider-over-top),var(--ootp-sb-slider-over-bottom),var(--ootp-sb-slider-over-mid)!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:active,.content::-webkit-scrollbar-thumb:vertical:active,.tablewrap::-webkit-scrollbar-thumb:vertical:active,.card::-webkit-scrollbar-thumb:vertical:active{background-image:var(--ootp-sb-slider-down-top),var(--ootp-sb-slider-down-bottom),var(--ootp-sb-slider-down-mid)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement,.content::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-less-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment,.content::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-more-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:hover,.content::-webkit-scrollbar-button:vertical:decrement:hover,.tablewrap::-webkit-scrollbar-button:vertical:decrement:hover,.card::-webkit-scrollbar-button:vertical:decrement:hover{background-image:var(--ootp-sb-less-over)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:hover,.content::-webkit-scrollbar-button:vertical:increment:hover,.tablewrap::-webkit-scrollbar-button:vertical:increment:hover,.card::-webkit-scrollbar-button:vertical:increment:hover{background-image:var(--ootp-sb-more-over)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:active,.content::-webkit-scrollbar-button:vertical:decrement:active,.tablewrap::-webkit-scrollbar-button:vertical:decrement:active,.card::-webkit-scrollbar-button:vertical:decrement:active{background-image:var(--ootp-sb-less-down)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:active,.content::-webkit-scrollbar-button:vertical:increment:active,.tablewrap::-webkit-scrollbar-button:vertical:increment:active,.card::-webkit-scrollbar-button:vertical:increment:active{background-image:var(--ootp-sb-more-down)!important}",
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
        ".dropdown::-webkit-scrollbar:vertical,.content::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:%dpx!important;background:transparent!important}"
        ".dropdown::-webkit-scrollbar:horizontal,.content::-webkit-scrollbar:horizontal,.tablewrap::-webkit-scrollbar:horizontal,.card::-webkit-scrollbar:horizontal{display:none!important;height:0!important}"
        ".dropdown::-webkit-scrollbar-track:vertical,.content::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical,.content::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px!important;background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:hover,.content::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:url('%s'),url('%s'),url('%s')!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:active,.content::-webkit-scrollbar-thumb:vertical:active,.tablewrap::-webkit-scrollbar-thumb:vertical:active,.card::-webkit-scrollbar-thumb:vertical:active{background-image:url('%s'),url('%s'),url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement,.content::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment,.content::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:hover,.content::-webkit-scrollbar-button:vertical:decrement:hover,.tablewrap::-webkit-scrollbar-button:vertical:decrement:hover,.card::-webkit-scrollbar-button:vertical:decrement:hover{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:hover,.content::-webkit-scrollbar-button:vertical:increment:hover,.tablewrap::-webkit-scrollbar-button:vertical:increment:hover,.card::-webkit-scrollbar-button:vertical:increment:hover{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:active,.content::-webkit-scrollbar-button:vertical:decrement:active,.tablewrap::-webkit-scrollbar-button:vertical:decrement:active,.card::-webkit-scrollbar-button:vertical:decrement:active{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:active,.content::-webkit-scrollbar-button:vertical:increment:active,.tablewrap::-webkit-scrollbar-button:vertical:increment:active,.card::-webkit-scrollbar-button:vertical:increment:active{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-corner,.content::-webkit-scrollbar-corner,.tablewrap::-webkit-scrollbar-corner,.card::-webkit-scrollbar-corner{background:transparent!important}",
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

static void kbo_webview_append_roster_table_css(KboWindowTextBuffer* css)
{
    if (css == NULL) {
        return;
    }
    kbo_window_text_appendf(
        css,
        ".tablewrap.rosterTableWrap{position:relative;border:1px solid #171717;border-radius:2px;background:#1b1b1b;box-shadow:inset 0 1px 0 rgba(255,255,255,.04);overflow-y:auto;overflow-x:hidden}"
        ".kboRosterScrollHost{position:relative}.rosterTableWrap.kboCustomScroll{scrollbar-width:none;scrollbar-gutter:auto!important}.rosterTableWrap.kboCustomScroll::-webkit-scrollbar{width:0!important;height:0!important;display:none!important}.rosterTableWrap.kboCustomScrollVisible{padding-right:var(--ootp-sb-width)}"
        ".kboOotpScrollbar{position:absolute;right:0;top:0;z-index:14;width:var(--ootp-sb-width);display:none;grid-template-rows:var(--ootp-sb-width) minmax(0,1fr) var(--ootp-sb-width);user-select:none;background:transparent}.kboRosterScrollHost>.kboOotpScrollbar{display:none}"
        ".kboOotpScrollButton{appearance:none;-webkit-appearance:none;width:var(--ootp-sb-width);height:var(--ootp-sb-width);margin:0;padding:0;border:0;border-radius:0;background-color:transparent;background-repeat:no-repeat;background-position:center center;cursor:pointer}.kboOotpScrollButton.less{background-image:var(--ootp-sb-less-up)}.kboOotpScrollButton.less:hover{background-image:var(--ootp-sb-less-over)}.kboOotpScrollButton.less:active{background-image:var(--ootp-sb-less-down)}.kboOotpScrollButton.more{background-image:var(--ootp-sb-more-up)}.kboOotpScrollButton.more:hover{background-image:var(--ootp-sb-more-over)}.kboOotpScrollButton.more:active{background-image:var(--ootp-sb-more-down)}"
        ".kboOotpScrollTrack{position:relative;min-height:0;background-color:transparent;background-image:var(--ootp-sb-bar-top),var(--ootp-sb-bar-bottom),var(--ootp-sb-bar-mid);background-repeat:no-repeat,no-repeat,repeat-y;background-position:center top,center bottom,center top}.kboOotpScrollThumb{position:absolute;left:0;right:0;top:0;min-height:20px;background-color:transparent;background-image:var(--ootp-sb-slider-up-top),var(--ootp-sb-slider-up-bottom),var(--ootp-sb-slider-up-mid);background-repeat:no-repeat,no-repeat,repeat-y;background-position:center top,center bottom,center top;cursor:pointer}.kboOotpScrollThumb:hover{background-image:var(--ootp-sb-slider-over-top),var(--ootp-sb-slider-over-bottom),var(--ootp-sb-slider-over-mid)}.kboOotpScrollThumb.dragging{background-image:var(--ootp-sb-slider-down-top),var(--ootp-sb-slider-down-bottom),var(--ootp-sb-slider-down-mid)}"
        ".ootpRosterTable{width:100%%;table-layout:fixed;border-collapse:separate;border-spacing:0;font-family:var(--ui-font);font-size:14px;line-height:1.1;color:#e7e7e7}"
        ".ootpRosterTable th{position:sticky;top:0;z-index:6;height:25px;padding:3px 7px;background:#252525;background-clip:padding-box;color:#eeeeee;border-top:1px solid #383838;border-bottom:1px solid #151515;text-align:left;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;box-shadow:0 1px 0 #151515,0 2px 0 #1b1b1b}"
        ".ootpRosterTable th[data-sort-type]{cursor:pointer;padding-right:18px}.ootpRosterTable th[data-sort-type]::after{content:'';position:absolute;right:6px;top:50%%;width:0;height:0;opacity:0}.ootpRosterTable th.sortAsc::after{margin-top:-4px;border-left:4px solid transparent;border-right:4px solid transparent;border-bottom:5px solid #dedede;opacity:.9}.ootpRosterTable th.sortDesc::after{margin-top:-2px;border-left:4px solid transparent;border-right:4px solid transparent;border-top:5px solid #dedede;opacity:.9}"
        ".ootpRosterTable td{height:27px;padding:3px 7px;background:#202020;color:#e4e4e4;border-top:1px solid rgba(255,255,255,.025);border-bottom:1px solid #161616;font-weight:400;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".ootpRosterTable tbody tr:nth-child(even) td{background:#232323}"
        ".ootpRosterTable tbody tr:hover td{background:#565656;color:#fff}"
        ".ootpRosterTable tbody tr.selected td{background:#4d4d4d;color:#fff}"
        ".ootpRosterTable .roPo{width:42px;text-align:left}.ootpRosterTable .roNum{width:34px;text-align:right;color:#e8e8e8}.ootpRosterTable .roName{width:210px;font-weight:500;color:#eeeeee}"
        ".ootpRosterTable .roLeague{width:250px}.ootpRosterTable .roAge{width:44px;text-align:right}.ootpRosterTable .roNat{width:110px}.roNatWrap{display:flex;align-items:center;gap:6px;min-width:0}.roNatFlag{width:20px;height:14px;object-fit:cover;flex:none;border:1px solid rgba(0,0,0,.48);box-shadow:0 0 0 1px rgba(255,255,255,.08)}.roNatText{min-width:0;overflow:hidden;text-overflow:ellipsis}.ootpRosterTable .roTeam{width:94px}.ootpRosterTable .roClub{width:160px}.ootpRosterTable .roReturn{width:118px}.ootpRosterTable .roEntry{width:96px;text-align:right}.ootpRosterTable .roStatus{width:98px}.ootpRosterTable .roSlot{width:58px}.ootpRosterTable .roResult{width:94px}.ootpRosterTable .roAction{width:110px}.ootpRosterTable .roDate{width:92px}"
        ".ootpRosterTable .roOrange{color:#ff7a00}.ootpRosterTable .roMuted{color:#bcbcbc}.ootpRosterTable .roReady{color:#49c5ff}.ootpRosterTable .roSoon{color:#ffb13b}.ootpRosterTable .roServing{color:#d7d7d7}"
        ".applicantRosterTable .roName{width:230px}.applicantRosterTable .roClub{width:190px}.applicantRosterTable .roStatus{color:#ff7a00;font-weight:500}.resultRosterTable .roName{width:240px}.resultRosterTable .roLeague{width:210px}.resultRosterTable .roResult{color:#ff7a00;font-weight:500}.foreignRosterTable .roName{width:230px}.foreignRosterTable .roNat{width:160px}.foreignRosterTable .roStatus{width:140px}.foreignRosterTable .roSlot{color:#ff7a00;font-weight:500}.foreignRightsTable .roAction{width:58px;text-align:center}.foreignRightsTable .roName{width:230px}.foreignRightsTable .roNat{width:150px}.foreignRightsTable .roStatus{width:140px}.foreignRightsTable .roTeam{width:82px}.faCasesTable .roName{width:220px}.faCasesTable .roStatus{width:340px}.faCases .metricRow{display:flex;align-items:center;gap:7px;min-height:28px;padding:0 8px 6px;overflow:hidden}.faCases .pill{display:inline-flex;align-items:center;min-width:0;height:22px;padding:0 8px;border:1px solid #343434;border-radius:4px;background:#202020;color:#e8e8e8;font-family:var(--ui-font);font-size:12px;font-weight:900;white-space:nowrap}.agTournamentTable .roPo{width:60px}.agTournamentTable .roName{width:220px}.agTournamentTable .roDate{width:190px}.agTournamentTable .roClub{width:116px}.agTournamentTable .roTeam{width:116px}.agTournamentTable .roReturn{width:116px}.agTournamentTable .roStatus{width:118px}.agTournamentTable .roResult{width:122px}.rightsActions{display:flex;align-items:center;justify-content:center;min-width:0}.rightsAction{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;border:0;border-radius:0;background-color:transparent;background-repeat:no-repeat;background-position:center center;background-size:24px 24px;color:transparent;font-size:0;line-height:0;text-decoration:none;cursor:pointer}.rightsRelease{background-image:var(--ootp-btn-minus-up)}.rightsRelease:hover{background-image:var(--ootp-btn-minus-over)}.rightsRelease:active{background-image:var(--ootp-btn-minus-down)}.ootpConfirmOverlay{position:fixed;inset:0;z-index:1200;display:none;align-items:center;justify-content:center;background:rgba(0,0,0,.46);font-family:var(--ui-font)}.ootpConfirmOverlay.show{display:flex}.ootpConfirmDialog{width:430px;max-width:calc(100vw - 36px);border:1px solid #383838;border-radius:5px;background:#1a1a1a;box-shadow:0 10px 28px rgba(0,0,0,.72),inset 0 1px 0 rgba(255,255,255,.05);overflow:hidden}.ootpConfirmTitle{height:31px;display:flex;align-items:center;padding:0 11px;background:#2a2a2a;color:#f2f2f2;font-size:14px;font-weight:900;border-bottom:1px solid #0f0f0f;text-transform:uppercase}.ootpConfirmBody{padding:14px 14px 10px;color:#e8e8e8;font-size:13px;font-weight:400;line-height:1.42}.ootpConfirmPlayer{margin-top:8px;padding:7px 9px;border:1px solid #313131;border-radius:3px;background:#101010;color:#fff;font-weight:800;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.ootpConfirmDetail{margin-top:8px;color:#a7a7a7}.ootpConfirmActions{display:flex;justify-content:flex-end;gap:8px;padding:0 14px 14px}.ootpConfirmButton{min-width:96px;height:28px;border:1px solid #414141;border-radius:4px;background:#262626;color:#eeeeee;font-family:var(--ui-font);font-size:13px;font-weight:900}.ootpConfirmButton:hover{border-color:#777;background:#303030}.ootpConfirmButton.primary{background:#3b2318;border-color:#7c3b24;color:#fff}.ootpConfirmButton.primary:hover{background:#562c1d;border-color:#a24d2d}.rosterRights{gap:0!important}.rosterTopBar{height:34px;display:flex;align-items:center;justify-content:flex-end;gap:12px;padding:0 8px 4px 8px;background:transparent;border:0;color:#e8e8e8;font-family:var(--ui-font);font-size:14px;font-weight:700;line-height:1;white-space:nowrap;overflow:hidden}.rosterTopText{margin-left:auto;min-width:0;max-width:100%%;overflow:hidden;text-overflow:ellipsis;text-align:right}.rosterTopControls{display:flex;align-items:center;justify-content:flex-end;gap:8px;flex:none}.rosterTopLabel{color:#f0f0f0;font-size:14px;font-weight:900;text-transform:uppercase}.rosterTopBar .rosterYearSelect{width:96px;height:24px;border:1px solid #c8c8c8;border-radius:4px;background:#ededed;color:#111;font-family:var(--ui-font);font-size:13px;font-weight:900;padding:0 24px 0 9px}.ootpRosterTable .roEmptyMessage{text-align:center;color:#dcdcdc;font-weight:500;background:#202020!important}");
    kbo_window_text_appendf(
        css,
        ".ootpConfirmOverlay{background:rgba(0,0,0,.56);align-items:center;justify-content:center}"
        ".ootpConfirmDialog{width:486px;max-width:calc(100vw - 28px);min-height:176px;border:1px solid #242428;border-radius:5px;background:#38383c;box-shadow:0 8px 24px rgba(0,0,0,.78);overflow:hidden}"
        ".ootpConfirmTitle{position:relative;height:35px;display:flex;align-items:center;justify-content:center;padding:0 54px;background:linear-gradient(#5d5d63,#4d4d53);border-bottom:1px solid rgba(0,0,0,.5);color:#111;font-size:22px;font-weight:900;line-height:35px;text-transform:none}"
        ".ootpConfirmQuestion{position:absolute;left:20px;top:0;height:35px;line-height:35px;color:#8d8d93;text-shadow:0 1px 0 rgba(255,255,255,.14);font-size:23px;font-weight:900}"
        ".ootpConfirmTitleText{display:block;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".ootpConfirmBody{min-height:95px;padding:30px 32px 12px;color:#f5f5f6;font-size:16px;font-weight:800;line-height:1.35}"
        ".ootpConfirmMessage{white-space:normal}.ootpConfirmPlayer,.ootpConfirmDetail{display:none}"
        ".ootpConfirmActions{height:46px;display:flex;align-items:flex-start;justify-content:flex-end;gap:14px;padding:0 33px 18px}"
        ".ootpConfirmButton,.ootpConfirmButton.primary{min-width:92px;height:26px;display:inline-flex;align-items:center;justify-content:center;gap:10px;border:0;border-radius:4px;background:#717079;color:#f3f3f3;box-shadow:inset 0 1px 0 rgba(255,255,255,.08);font-family:var(--ui-font);font-size:14px;font-weight:800;line-height:26px}"
        ".ootpConfirmButton:hover,.ootpConfirmButton.primary:hover{background:#7b7a82}.ootpConfirmButton:active,.ootpConfirmButton.primary:active{background:#64636b}"
        ".ootpConfirmButton:focus{outline:1px solid rgba(255,255,255,.38);outline-offset:1px}"
        ".ootpConfirmButtonIcon{display:inline-block;width:16px;color:#050505;font-family:'Segoe UI Symbol','Arial',sans-serif;font-size:20px;font-weight:900;line-height:20px;text-align:center}");
}

static void kbo_webview_append_roster_sort_script(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    const char* confirm_title = kbo_hub_text("\xec\xa7\x88\xeb\xac\xb8", "Question");
    const char* confirm_message = kbo_hub_text(
        "\xec\xa0\x95\xeb\xa7\x90\xeb\xa1\x9c %s\xec\x9d\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c\xec\x9d\x84 \xed\x95\xb4\xec\xa0\x9c\xed\x95\x98\xec\x8b\x9c\xea\xb2\xa0\xec\x8a\xb5\xeb\x8b\x88\xea\xb9\x8c?",
        "Do you really want to release %s?");
    const char* confirm_player = kbo_hub_text("\xec\x9d\xb4 \xec\x84\xa0\xec\x88\x98", "this player");
    const char* confirm_cancel = kbo_hub_text("\xec\xb7\xa8\xec\x86\x8c", "Cancel");
    const char* confirm_ok = kbo_hub_text("\xed\x99\x95\xec\x9d\xb8", "OK");
    kbo_window_text_appendf(
        buffer,
        "<script>"
        "(function(){"
        "function isEditableTarget(node){while(node&&node!==document){var tag=(node.tagName||'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select'||node.isContentEditable){return true;}node=node.parentNode;}return false;}"
        "function installGameSurfaceGuards(){document.addEventListener('selectstart',function(e){if(!isEditableTarget(e.target)){e.preventDefault();}},true);document.addEventListener('dragstart',function(e){e.preventDefault();},true);document.addEventListener('mousedown',function(e){if(e.detail>1&&!isEditableTarget(e.target)){e.preventDefault();}},true);var images=document.querySelectorAll('img');for(var i=0;i<images.length;i++){images[i].setAttribute('draggable','false');}}"
        "function textOf(cell){return (cell&&cell.textContent?cell.textContent:'').trim();}"
        "function numericValue(text){var clean='';for(var i=0;i<text.length;i++){var ch=text.charAt(i);if((ch>='0'&&ch<='9')||ch==='.'||ch==='-'){clean+=ch;}}var value=parseFloat(clean);return isNaN(value)?null:value;}"
        "function comparable(cell,type){var raw=(cell&&cell.getAttribute)?cell.getAttribute('data-sort-value'):null;var text=(raw!==null&&raw!=='')?raw:textOf(cell);if(type==='number'){var value=numericValue(text);return {empty:value===null,value:value===null?0:value};}return {empty:text.length===0,value:text.toLowerCase()};}"
        "function clearSort(table){var headers=table.querySelectorAll('th.sortAsc,th.sortDesc');for(var i=0;i<headers.length;i++){headers[i].classList.remove('sortAsc');headers[i].classList.remove('sortDesc');}}"
        "function sortTable(th){var table=th.closest('table');if(!table||!table.tBodies.length){return;}var body=table.tBodies[0];var column=th.cellIndex;var type=th.getAttribute('data-sort-type')||'text';var dir=th.classList.contains('sortAsc')?'desc':'asc';var rows=Array.prototype.slice.call(body.rows).filter(function(row){return row.cells.length>column&&!row.cells[0].hasAttribute('colspan');}).map(function(row,index){return {row:row,index:index};});"
        "rows.sort(function(a,b){var av=comparable(a.row.cells[column],type);var bv=comparable(b.row.cells[column],type);if(av.empty!==bv.empty){return av.empty?1:-1;}var cmp=0;if(type==='number'){cmp=av.value-bv.value;}else{cmp=av.value.localeCompare(bv.value,undefined,{numeric:true,sensitivity:'base'});}if(cmp===0){cmp=a.index-b.index;}return dir==='asc'?cmp:-cmp;});"
        "clearSort(table);th.classList.add(dir==='asc'?'sortAsc':'sortDesc');for(var i=0;i<rows.length;i++){body.appendChild(rows[i].row);}var scroller=table.closest('.rosterTableWrap');if(scroller&&scroller.kboOotpUpdate){scroller.kboOotpUpdate();}}"
        "function makeButton(cls){var button=document.createElement('button');button.type='button';button.className='kboOotpScrollButton '+cls;button.tabIndex=-1;return button;}"
        "function installOotpScrollbar(scroller){if(!scroller||scroller.getAttribute('data-kbo-scrollbar')==='1'){return;}var host=scroller.parentElement;if(!host){return;}scroller.setAttribute('data-kbo-scrollbar','1');scroller.classList.add('kboCustomScroll');host.classList.add('kboRosterScrollHost');var bar=document.createElement('div');bar.className='kboOotpScrollbar';var less=makeButton('less');var track=document.createElement('div');track.className='kboOotpScrollTrack';var thumb=document.createElement('div');thumb.className='kboOotpScrollThumb';var more=makeButton('more');track.appendChild(thumb);bar.appendChild(less);bar.appendChild(track);bar.appendChild(more);host.appendChild(bar);var dragging=false;var dragStartY=0;var dragStartTop=0;"
        "function layoutBar(){var hr=host.getBoundingClientRect();var sr=scroller.getBoundingClientRect();bar.style.top=Math.round(sr.top-hr.top)+'px';bar.style.height=Math.round(sr.height)+'px';bar.style.right=Math.round(hr.right-sr.right)+'px';}"
        "function metrics(){layoutBar();var maxScroll=scroller.scrollHeight-scroller.clientHeight;var trackH=track.clientHeight;var buttonH=less.offsetHeight||20;var thumbH=maxScroll>1?Math.max(buttonH,Math.round(trackH*scroller.clientHeight/scroller.scrollHeight)):trackH;if(thumbH>trackH){thumbH=trackH;}var maxTop=Math.max(0,trackH-thumbH);var top=maxScroll>1?Math.round(maxTop*scroller.scrollTop/maxScroll):0;return {maxScroll:maxScroll,trackH:trackH,thumbH:thumbH,maxTop:maxTop,top:top};}"
        "function update(){bar.style.display='grid';var m=metrics();var visible=m.maxScroll>1&&m.trackH>0;scroller.classList.toggle('kboCustomScrollVisible',visible);bar.style.display=visible?'grid':'none';thumb.style.height=m.thumbH+'px';thumb.style.top=m.top+'px';}"
        "function scrollByAmount(direction,page){var step=page?Math.max(27,Math.floor(scroller.clientHeight*.85)):27;scroller.scrollTop+=direction*step;}"
        "less.addEventListener('click',function(e){e.preventDefault();scrollByAmount(-1,false);});more.addEventListener('click',function(e){e.preventDefault();scrollByAmount(1,false);});"
        "bar.addEventListener('wheel',function(e){e.preventDefault();scroller.scrollTop+=e.deltaY;},{passive:false});"
        "track.addEventListener('mousedown',function(e){if(e.target===thumb){return;}var rect=track.getBoundingClientRect();var m=metrics();var y=e.clientY-rect.top;if(y<m.top){scrollByAmount(-1,true);}else if(y>m.top+m.thumbH){scrollByAmount(1,true);}});"
        "thumb.addEventListener('mousedown',function(e){e.preventDefault();dragging=true;thumb.classList.add('dragging');dragStartY=e.clientY;dragStartTop=metrics().top;document.body.style.userSelect='none';});"
        "document.addEventListener('mousemove',function(e){if(!dragging){return;}var m=metrics();if(m.maxTop<=0||m.maxScroll<=0){return;}var nextTop=Math.max(0,Math.min(m.maxTop,dragStartTop+(e.clientY-dragStartY)));scroller.scrollTop=nextTop*m.maxScroll/m.maxTop;});"
        "document.addEventListener('mouseup',function(){if(!dragging){return;}dragging=false;thumb.classList.remove('dragging');document.body.style.userSelect='';});"
        "scroller.addEventListener('scroll',update);window.addEventListener('resize',update);scroller.kboOotpUpdate=update;setTimeout(update,0);setTimeout(update,150);}"
        "function makeConfirmButton(cls,icon,text){var button=document.createElement('button');button.type='button';button.className='ootpConfirmButton '+cls;var mark=document.createElement('span');mark.className='ootpConfirmButtonIcon';mark.textContent=icon;var label=document.createElement('span');label.textContent=text;button.appendChild(mark);button.appendChild(label);return button;}"
        "function formatConfirmQuestion(template,name){return template.indexOf('%%s')>=0?template.replace('%%s',name):template;}"
        "function installRightsConfirm(){var pendingHref='';var messageTemplate=");
    kbo_webview_append_js_string(buffer, confirm_message);
    kbo_window_text_appendf(
        buffer,
        ";var overlay=document.createElement('div');overlay.className='ootpConfirmOverlay';var dialog=document.createElement('div');dialog.className='ootpConfirmDialog';var title=document.createElement('div');title.className='ootpConfirmTitle';var question=document.createElement('span');question.className='ootpConfirmQuestion';question.textContent='?';var titleText=document.createElement('span');titleText.className='ootpConfirmTitleText';titleText.textContent=");
    kbo_webview_append_js_string(buffer, confirm_title);
    kbo_window_text_appendf(
        buffer,
        ";title.appendChild(question);title.appendChild(titleText);var body=document.createElement('div');body.className='ootpConfirmBody';var message=document.createElement('div');message.className='ootpConfirmMessage';var actions=document.createElement('div');actions.className='ootpConfirmActions';var ok=makeConfirmButton('primary','\\u2713',");
    kbo_webview_append_js_string(buffer, confirm_ok);
    kbo_window_text_appendf(
        buffer,
        ");var cancel=makeConfirmButton('','\\u2715',");
    kbo_webview_append_js_string(buffer, confirm_cancel);
    kbo_window_text_appendf(
        buffer,
        ");actions.appendChild(ok);actions.appendChild(cancel);body.appendChild(message);dialog.appendChild(title);dialog.appendChild(body);dialog.appendChild(actions);overlay.appendChild(dialog);document.body.appendChild(overlay);"
        "function close(){overlay.classList.remove('show');pendingHref='';}"
        "function open(link){pendingHref=link.getAttribute('href')||'';var name=link.getAttribute('data-player')||");
    kbo_webview_append_js_string(buffer, confirm_player);
    kbo_window_text_appendf(
        buffer,
        ";message.textContent=formatConfirmQuestion(messageTemplate,name);overlay.classList.add('show');cancel.focus();}"
        "document.addEventListener('click',function(e){var node=e.target;while(node&&node!==document&&!(node.classList&&node.classList.contains('rightsRelease'))){node=node.parentNode;}if(!node||node===document){return;}e.preventDefault();open(node);});"
        "cancel.addEventListener('click',function(){close();});overlay.addEventListener('click',function(e){if(e.target===overlay){close();}});ok.addEventListener('click',function(){var href=pendingHref;close();if(href){window.location.href=href;}});document.addEventListener('keydown',function(e){if(e.key==='Escape'&&overlay.classList.contains('show')){close();}});}"
        "installGameSurfaceGuards();"
        "var headers=document.querySelectorAll('.ootpRosterTable th[data-sort-type]');for(var i=0;i<headers.length;i++){headers[i].addEventListener('click',function(){sortTable(this);});}"
        "var rosters=document.querySelectorAll('.rosterTableWrap');for(var r=0;r<rosters.length;r++){installOotpScrollbar(rosters[r]);}"
        "installRightsConfirm();"
        "})();"
        "</script>");
}

static WCHAR* kbo_build_webview_hub_html(void)
{
    const size_t html_cap = 1572864;
    char* html = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, html_cap);
    if (html == NULL) {
        return NULL;
    }
    KboWindowTextBuffer buffer;
    buffer.data = html;
    buffer.capacity = html_cap;
    buffer.length = 0;

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA) {
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
    }
    kbo_hub_ensure_valid_selection();

    char league_name[96] = {0};
    char team_name[96] = {0};
    char league_logo_path[MAX_PATH] = {0};
    char team_logo_path[MAX_PATH] = {0};
    char jeju_font_path[MAX_PATH] = {0};
    char jeju_font_url[MAX_PATH * 3] = {0};
    char team_bar_primary[8] = "#f04a22";
    char team_bar_secondary[8] = "#2c2c2c";
    char current_date_text[64] = {0};
    char window_status[256] = {0};
    char scrollbar_css[65536] = {0};
    const int is_mod_dashboard =
        g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO &&
        (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_README ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_LICENSE ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CREDITS ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_SETTINGS);
    const int is_roster_dashboard =
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY &&
         (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_ROSTER ||
          g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_APPLICANTS ||
          g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_RESULTS)) ||
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) ||
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA &&
         (g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_ROSTER ||
          g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS)) ||
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES &&
         (g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS ||
          g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_SCHEDULE ||
          g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_ROSTER)) ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES;
    const int is_dashboard_panel =
        is_mod_dashboard ||
        is_roster_dashboard ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS;
    const int has_sub_tabs = kbo_webview_current_view_has_sub_tabs();
    const char* ui_font_family = g_kbo_hub_language == KBO_HUB_LANG_KO
        ? "'KBO Jeju Gothic','Jeju Gothic','Malgun Gothic',sans-serif"
        : "'Malgun Gothic',sans-serif";
    kbo_hub_copy_league_display_name(g_kbo_hub_selected_league_id, league_name, sizeof(league_name));
    kbo_hub_copy_team_display_name_by_id(g_kbo_hub_selected_team_id, team_name, sizeof(team_name), "No team");
    kbo_get_foreign_waiver_window_status_text(window_status, sizeof(window_status));

    uint32_t current_year = 0, current_month = 0, current_day = 0;
    if (kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        kbo_hub_format_ootp_date(current_year, current_month, current_day, current_date_text, sizeof(current_date_text));
    } else {
        snprintf(current_date_text, sizeof(current_date_text), "DATE UNKNOWN");
    }
    kbo_hub_get_league_logo_path(g_kbo_hub_selected_league_id, current_year, league_logo_path, sizeof(league_logo_path));
    kbo_hub_get_team_logo_path(g_kbo_hub_selected_team_id, current_year, team_logo_path, sizeof(team_logo_path));
    kbo_hub_font_asset_path("JejuGothic-Regular.ttf", jeju_font_path, sizeof(jeju_font_path));
    kbo_webview_copy_file_url(jeju_font_path, jeju_font_url, sizeof(jeju_font_url));
    kbo_hub_copy_team_bar_colors(
        g_kbo_hub_selected_team_id,
        team_bar_primary,
        sizeof(team_bar_primary),
        team_bar_secondary,
        sizeof(team_bar_secondary));
    kbo_webview_build_scrollbar_skin_css(scrollbar_css, sizeof(scrollbar_css));
    KboWindowTextBuffer extra_css;
    extra_css.data = scrollbar_css;
    extra_css.capacity = sizeof(scrollbar_css);
    extra_css.length = strlen(scrollbar_css);
    kbo_webview_append_roster_table_css(&extra_css);

    kbo_window_text_appendf(&buffer,
        "<!doctype html><html><head><meta charset='utf-8'><style>"
        "@font-face{font-family:'KBO Jeju Gothic';font-style:normal;font-weight:400;src:url('%s') format('truetype')}"
        ":root{--bg:#0a0a0a;--header:#1d556c;--nav:#141414;--active:#1d556c;--panel:#161616;--panel2:#222;--ink:#fcfcfc;--muted:#8f8f8f;--orange:#de6d1f;--gold:#d6a44b;--line:rgba(255,255,255,.12);--team-primary:%s;--team-secondary:%s;--ui-font:%s}"
        "*{box-sizing:border-box;-webkit-user-select:none;user-select:none;-webkit-user-drag:none}html,body{height:100%;margin:0;overflow:hidden}body{background:var(--bg);color:var(--ink);font-family:var(--ui-font);font-size:%dpx;cursor:default}a{text-decoration:none;color:inherit;-webkit-user-drag:none}img{-webkit-user-drag:none;user-select:none}input,textarea,select{-webkit-user-select:auto;user-select:auto}.ootpRosterTable th[data-sort-type],a,button,.select,.ddItem,.switch,.action,.mainTab,.subTab{cursor:pointer}"
        ".app{height:100%;display:grid;grid-template-rows:64px 1fr;background:linear-gradient(135deg,#0a0a0a 0%%,#10171a 45%%,#080808 100%%)}"
        ".top{background:var(--header);display:flex;align-items:center;justify-content:space-between;padding:0 18px 0 18px;border-bottom:1px solid rgba(255,255,255,.18)}"
        ".identity{display:flex;align-items:center;gap:10px;min-width:0}.logo{width:46px;height:46px;object-fit:contain;filter:drop-shadow(0 1px 1px rgba(0,0,0,.65))}.brand{font-family:var(--ui-font);font-weight:800;font-size:%dpx;color:#f5f1e7;line-height:1}.date{font-family:var(--ui-font);font-size:%dpx;font-weight:800;color:#cfd5d6;margin-top:4px;text-transform:uppercase;letter-spacing:0}.brandBlock{min-width:0}"
        ".selects{display:flex;gap:10px}.select{min-width:162px;height:34px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 10px;border:1px solid rgba(255,255,255,.22);border-radius:4px;background:rgba(0,0,0,.16);color:#e6e6e8;font-family:var(--ui-font);font-weight:800}.select img{width:26px;height:26px;object-fit:contain;flex:none}.select span:first-of-type{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".dropdown{position:absolute;z-index:20;top:72px;max-height:420px;overflow-y:auto;overflow-x:hidden;background:#242424;border:1px solid #333;border-radius:3px;box-shadow:0 8px 18px rgba(0,0,0,.55);padding:4px 0;scrollbar-gutter:stable}.leagueMenu{right:190px;width:304px}.teamMenu{right:12px;width:304px}.ddItem{height:24px;display:flex!important;flex-direction:row!important;align-items:center;justify-content:flex-start;gap:6px;padding:0 10px 0 5px;color:#f2f2f2;font-family:var(--ui-font);font-size:16px;font-weight:800;line-height:24px;white-space:nowrap}.ddItem:hover,.ddItem.selected{background:#30434b}.ddLogo{width:20px;height:24px;display:inline-flex;align-items:center;justify-content:center;flex:0 0 20px;overflow:hidden}.ddLogo img{display:block;width:auto;height:auto;max-width:18px!important;max-height:18px!important;object-fit:contain}.ddText{display:block;flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dropdown::-webkit-scrollbar:vertical{width:20px}.dropdown::-webkit-scrollbar:horizontal{display:none;height:0}.dropdown::-webkit-scrollbar-track:vertical{background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_bar_mid.png') repeat-y center top}.dropdown::-webkit-scrollbar-thumb:vertical{min-height:42px;background:#2a2a2a url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_up_mid.png') repeat-y center top}.dropdown::-webkit-scrollbar-button:vertical:decrement{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_less_up.png') no-repeat center center}.dropdown::-webkit-scrollbar-button:vertical:increment{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_more_up.png') no-repeat center center}"
        ".panel{margin:18px 16px 16px;background:var(--panel);border:1px solid var(--panel2);border-radius:5px;min-height:0;height:calc(100%% - 34px);display:grid;grid-template-rows:66px 1fr;overflow:hidden}.panelHead{background:var(--panel2);padding:10px 15px}.panelHead h1{margin:0;font-family:var(--ui-font);font-size:%dpx;font-weight:800}.panelHead p{margin:3px 0 0;color:var(--muted)}"
        ".content{padding:16px;min-height:0;height:100%%;display:flex;flex-direction:column;overflow:hidden}.content::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:20px}.content::-webkit-scrollbar:horizontal,.tablewrap::-webkit-scrollbar:horizontal,.card::-webkit-scrollbar:horizontal{display:none;height:0}.content::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_bar_mid.png') repeat-y center top}.content::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px;background:#2a2a2a url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_up_mid.png') repeat-y center top}.content::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_over_mid.png')}.content::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_less_up.png') no-repeat center center}.content::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_more_up.png') no-repeat center center}.content::-webkit-scrollbar-corner,.tablewrap::-webkit-scrollbar-corner,.card::-webkit-scrollbar-corner{background:#101010}.rights,.card{flex:1;min-height:0;height:100%%}.rights{display:flex;flex-direction:column;gap:10px}.card{overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010;padding:16px}.reportbar{display:none}.muted{color:var(--muted);line-height:1.42}.help{display:flex;gap:8px;align-items:center;color:#b9b9b9}.help span{background:#242424;border:1px solid #343434;border-radius:4px;padding:4px 8px}.actions{display:flex;gap:8px;justify-content:flex-end}.action{display:inline-block;min-width:128px;text-align:center;color:#f4f4f4;border:1px solid #3a3a3a;border-radius:4px;background:#262626;font-family:var(--ui-font);font-weight:800;padding:8px 12px}.keep{background:#8d4b17;border-color:#c46b22}.release{background:#1d556c;border-color:#2e7896}.toggle{width:52px;text-align:center}.switch{display:inline-block;width:18px;height:18px;line-height:16px;margin-right:4px;text-align:center;color:#aaa;border:1px solid #343434;border-radius:3px;background:#181818;font-family:'Segoe UI Symbol',var(--ui-font);font-size:11px;font-weight:400;text-decoration:none}.switch:hover{color:#f0f0f0;border-color:#696969;background:#242424}.switch.keep,.switch.release{background:#181818;border-color:#343434}.tablewrap{flex:1;min-height:0;overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010}.modReadme{display:grid!important;grid-template-columns:minmax(0,1.45fr) minmax(260px,.85fr);grid-template-rows:minmax(220px,1fr) minmax(150px,.62fr);gap:12px;height:100%%!important;min-height:0}.modReadme .card{height:auto!important;min-height:0;overflow:hidden;scrollbar-gutter:auto;padding:14px 14px 16px;background:linear-gradient(135deg,#1d2020 0%%,#181818 58%%,#202020 100%%);border:1px solid rgba(255,255,255,.04);border-radius:5px;box-shadow:inset 0 1px 0 rgba(255,255,255,.03)}.modContrib{grid-template-rows:minmax(150px,.68fr) minmax(220px,1fr)}.settingsGrid{display:grid!important;grid-template-columns:minmax(0,1fr);grid-template-rows:minmax(0,1fr);gap:12px;height:100%%!important;min-height:0;align-content:stretch}.settingsCard{height:100%%!important;min-height:0;padding:14px 14px 16px;background:#181818;border:1px solid #292929;border-radius:5px;box-shadow:none}.settingRow{display:grid;grid-template-columns:150px minmax(180px,360px);align-items:center;justify-content:start;gap:12px;margin-top:4px}.settingLabel{color:#9c9c9c;font-size:13px;font-weight:800;white-space:nowrap}.ootpSelect{width:100%%;height:28px;border:1px solid #3c3c3c;border-radius:4px;background:#202020;color:#f0f0f0;font-family:var(--ui-font);font-size:13px;font-weight:700;padding:0 8px}.ootpSelect:focus{outline:1px solid #777;outline-offset:0}.modCard{display:flex;flex-direction:column}.modCardMain{grid-row:1/span 2}.cardTitle{margin:0 0 12px;color:#9c9c9c;font-size:16px;font-weight:900;line-height:1.1;text-transform:uppercase}.modCard p{margin:0 0 10px;color:#eeeeee;line-height:1.48;white-space:normal;font-size:14px;font-weight:400}.modCard p:last-child{margin-bottom:0}.modLead{font-size:15px!important;font-weight:400!important;color:#fff!important}.buildList{display:grid;gap:6px;margin-top:2px;align-content:start}.buildRow{display:grid;grid-template-columns:88px minmax(0,1fr);gap:10px;align-items:baseline;color:#e5e5e5;font-size:13px;line-height:1.35}.buildLabel{color:#8f8f8f;font-weight:900;white-space:nowrap}.buildValue{font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.githubHero{display:flex;align-items:center;gap:13px;margin:2px 0 12px}.githubLogo{width:58px;height:58px;object-fit:contain;filter:invert(54%%) grayscale(1);opacity:.78;padding:4px;flex:none}.githubRepo{min-width:0}.githubRepo strong{display:block;font-size:17px;color:#f4f4f4;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.githubRepo span{display:block;margin-top:4px;color:#9e9e9e;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.githubLink{display:inline-flex;align-items:center;justify-content:center;align-self:flex-start;margin-top:auto;min-width:128px;height:32px;border:1px solid #3a3a3a;border-radius:4px;background:#262626;color:#f4f4f4;font-weight:900}.githubLink:hover{border-color:#686868;background:#303030}table{width:100%%;table-layout:fixed;border-collapse:separate;border-spacing:0;font-family:var(--ui-font);font-size:13px}th{position:sticky;top:0;background:#222;color:#dedede;text-align:left;padding:8px 10px;border-bottom:1px solid #393939;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}td{padding:7px 10px;border-bottom:1px solid #242424;color:#d8d8d8;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}thead th:first-child{border-top-left-radius:3px}thead th:last-child{border-top-right-radius:3px}tbody tr:last-child td:first-child{border-bottom-left-radius:3px}tbody tr:last-child td:last-child{border-bottom-right-radius:3px}tr:nth-child(even) td{background:#151515}tr:hover td,tr.selected td{background:#243b45;color:#fff}.sel{width:48px;color:var(--orange);font-weight:900;white-space:nowrap}.pname{width:156px;max-width:156px;overflow:hidden;text-overflow:ellipsis;font-weight:800;color:#f3f0e8}.team{width:64px;max-width:64px;text-align:left;font-weight:800;color:#ddd}.flag{width:44px;text-align:center;cursor:help}.flag .roNatFlag{vertical-align:middle}.empty{border:1px dashed rgba(255,255,255,.22);border-radius:4px;padding:22px;color:var(--muted);background:#141414;white-space:normal}"
        ".app{height:100vh;grid-template-rows:96px 42px 40px minmax(0,1fr);background:#111}.app.noSubTabs{grid-template-rows:96px 42px minmax(0,1fr)}.top{background:transparent;justify-content:flex-start;gap:18px;padding:0 24px;border-bottom:0}.identity{gap:18px}.logo{width:74px;height:74px}.brand{font-size:%dpx;letter-spacing:0;color:#dcdcdc}.brand span{color:#8f8f8f}.date{font-size:%dpx;color:#c9c9c9}.date a{color:#c9c9c9}.selects{margin-left:auto}.select{height:30px;min-width:150px;border-color:rgba(255,255,255,.16);background:rgba(0,0,0,.24)}.mainTabs{display:flex;align-items:center;align-self:end;height:38px;margin:4px 6px 0;background:var(--team-primary);border-bottom:1px solid rgba(0,0,0,.42);border-radius:5px 5px 0 0;overflow:hidden;padding:0 10px;gap:8px}.mainTab{display:flex;align-items:center;justify-content:center;height:26px;min-width:112px;padding:0 16px;color:#fff;font-family:var(--ui-font);font-size:15px;font-weight:900;text-transform:uppercase;border:1px solid transparent;border-radius:4px}.mainTab.active{background:rgba(255,255,255,.14);border-color:rgba(255,255,255,.78);color:#fff}.subTabs{display:flex;align-items:center;align-self:start;height:38px;margin:0 6px 2px;background:var(--team-secondary);border-bottom:1px solid rgba(0,0,0,.58);border-radius:0 0 5px 5px;overflow:hidden;padding:0 16px;gap:16px}.subTab{height:24px;line-height:23px;padding:0 18px;color:#e6e6e6;font-family:var(--ui-font);font-size:15px;font-weight:900;text-transform:uppercase;border:1px solid transparent;border-radius:4px}.subTab.active{border-color:#d8d8d8;background:rgba(255,255,255,.13);color:#fff}.panel{height:auto!important;min-height:0!important;margin:12px 14px 14px;align-self:stretch;grid-template-rows:54px minmax(0,1fr);border-radius:4px}.panelHead{padding:8px 14px}.panelHead h1{font-size:%dpx}.panelHead p{font-size:%dpx}.panel.dashboardPanel{margin:6px 8px 8px;background:transparent;border:0;border-radius:0;box-shadow:none;grid-template-rows:minmax(0,1fr);overflow:visible}.dashboardPanel .panelHead{display:none}.dashboardPanel .content{display:block;height:100%%!important;padding:0;overflow:hidden!important;scrollbar-gutter:auto}.content{height:auto!important;min-height:0;overflow-y:auto!important;overflow-x:hidden!important;scrollbar-gutter:stable}.rights{height:100%%!important;min-height:0}.tablewrap{flex:1 1 auto;min-height:0;scrollbar-gutter:stable}.card{height:100%%!important;min-height:0;scrollbar-gutter:stable}.dropdown{top:92px}.leagueMenu{left:140px;right:auto}.teamMenu{left:140px;right:auto}"
        "%s</style></head><body><div class='app %s'><header class='top'><div class='identity'>",
        jeju_font_url,
        team_bar_primary,
        team_bar_secondary,
        ui_font_family,
        g_kbo_hub_skin_article_font_px - 1,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_button_font_px - 4,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_article_font_px + 10,
        g_kbo_hub_skin_button_font_px - 3,
        g_kbo_hub_skin_article_font_px + 4,
        g_kbo_hub_skin_article_font_px - 2,
        scrollbar_css,
        has_sub_tabs ? "hasSubTabs" : "noSubTabs");
    const char* header_logo_path = team_logo_path[0] != '\0' ? team_logo_path : league_logo_path;
    if (header_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img class='logo' src='");
        kbo_webview_append_image_src(&buffer, header_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<div class='brandBlock'><a class='brand' href='kbo://team'>");
    kbo_html_append_escaped(&buffer, team_name);
    kbo_window_text_appendf(&buffer, " <span>v</span></a><div class='date'><a href='kbo://league'>");
    kbo_html_append_escaped(&buffer, league_name);
    kbo_window_text_appendf(&buffer, "</a> / ");
    kbo_html_append_escaped(&buffer, current_date_text);
    kbo_window_text_appendf(&buffer, "</div></div></div></header>");
    if (g_kbo_hub_open_dropdown == 1) {
        kbo_webview_append_league_dropdown(&buffer, current_year);
    } else if (g_kbo_hub_open_dropdown == 2) {
        kbo_webview_append_team_dropdown(&buffer, current_year);
    }

    kbo_window_text_appendf(&buffer, "<nav class='mainTabs'>");
    kbo_webview_append_main_tabs(&buffer);
    kbo_window_text_appendf(&buffer, "</nav>");
    if (has_sub_tabs) {
        kbo_window_text_appendf(&buffer, "<nav class='subTabs'>");
        kbo_webview_append_sub_tabs(&buffer);
        kbo_window_text_appendf(&buffer, "</nav>");
    }
    kbo_window_text_appendf(
        &buffer,
        "<main class='panel %s'><div class='panelHead'><h1>",
        is_dashboard_panel ? "dashboardPanel" : "");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_title());
    kbo_window_text_appendf(&buffer, "</h1><p>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_subtitle());
    kbo_window_text_appendf(&buffer, "</p></div><section class='content'>");

    kbo_webview_append_selected_view(&buffer, current_year, window_status);
    kbo_window_text_appendf(&buffer, "</section></main></div>");
    kbo_webview_append_roster_sort_script(&buffer);
    kbo_window_text_appendf(&buffer, "</body></html>");

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, html, -1, NULL, 0);
    WCHAR* wide = NULL;
    if (wide_len > 0) {
        wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            MultiByteToWideChar(CP_UTF8, 0, html, -1, wide, wide_len);
        }
    }
    HeapFree(GetProcessHeap(), 0, html);
    return wide;
}

static void kbo_webview_navigate_current(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }
    WCHAR* html = kbo_build_webview_hub_html();
    if (html != NULL) {
        ICoreWebView2_NavigateToString(g_kbo_webview, html);
        HeapFree(GetProcessHeap(), 0, html);
    }
}

