#include "ui_html_render/view_military.inc"
#include "ui_html_render/view_foreign_rights.inc"
#include "ui_html_render/view_asian_quota.inc"
#include "ui_html_render/view_asian_games.inc"
#include "ui_html_render/view_fallback.inc"
#include "ui_html_render/view_router.inc"

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

    kbo_hub_ensure_valid_selection();

    char league_name[96] = {0};
    char team_name[96] = {0};
    char league_logo_path[MAX_PATH] = {0};
    char team_logo_path[MAX_PATH] = {0};
    char current_date_text[64] = {0};
    char window_status[256] = {0};
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

    kbo_window_text_appendf(&buffer,
        "<!doctype html><html><head><meta charset='utf-8'><style>"
        ":root{--bg:#0a0a0a;--header:#1d556c;--nav:#141414;--active:#1d556c;--panel:#161616;--panel2:#222;--ink:#fcfcfc;--muted:#8f8f8f;--orange:#de6d1f;--gold:#d6a44b;--line:rgba(255,255,255,.12)}"
        "*{box-sizing:border-box}html,body{height:100%;margin:0;overflow:hidden}body{background:var(--bg);color:var(--ink);font-family:'Malgun Gothic';font-size:%dpx}a{text-decoration:none;color:inherit}"
        ".app{height:100%;display:grid;grid-template-rows:64px 1fr;background:linear-gradient(135deg,#0a0a0a 0%%,#10171a 45%%,#080808 100%%)}"
        ".top{background:var(--header);display:flex;align-items:center;justify-content:space-between;padding:0 18px 0 18px;border-bottom:1px solid rgba(255,255,255,.18)}"
        ".identity{display:flex;align-items:center;gap:10px;min-width:0}.logo{width:46px;height:46px;object-fit:contain;filter:drop-shadow(0 1px 1px rgba(0,0,0,.65))}.brand{font-family:'Malgun Gothic';font-weight:800;font-size:%dpx;color:#f5f1e7;line-height:1}.date{font-family:'Malgun Gothic';font-size:%dpx;font-weight:800;color:#cfd5d6;margin-top:4px;text-transform:uppercase;letter-spacing:0}.brandBlock{min-width:0}"
        ".selects{display:flex;gap:10px}.select{min-width:162px;height:34px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 10px;border:1px solid rgba(255,255,255,.22);border-radius:4px;background:rgba(0,0,0,.16);color:#e6e6e8;font-family:'Malgun Gothic';font-weight:800}.select img{width:26px;height:26px;object-fit:contain;flex:none}.select span:first-of-type{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".dropdown{position:absolute;z-index:20;top:72px;max-height:420px;overflow-y:auto;overflow-x:hidden;background:#242424;border:1px solid #333;border-radius:3px;box-shadow:0 8px 18px rgba(0,0,0,.55);padding:4px 0;scrollbar-gutter:stable}.leagueMenu{right:190px;width:304px}.teamMenu{right:12px;width:304px}.ddItem{height:24px;display:flex!important;flex-direction:row!important;align-items:center;justify-content:flex-start;gap:6px;padding:0 10px 0 5px;color:#f2f2f2;font-family:'Malgun Gothic';font-size:16px;font-weight:800;line-height:24px;white-space:nowrap}.ddItem:hover,.ddItem.selected{background:#30434b}.ddLogo{width:20px;height:24px;display:inline-flex;align-items:center;justify-content:center;flex:0 0 20px;overflow:hidden}.ddLogo img{display:block;width:auto;height:auto;max-width:18px!important;max-height:18px!important;object-fit:contain}.ddText{display:block;flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dropdown::-webkit-scrollbar:vertical{width:20px}.dropdown::-webkit-scrollbar:horizontal{display:none;height:0}.dropdown::-webkit-scrollbar-track:vertical{background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_bar_mid.png') repeat-y center top}.dropdown::-webkit-scrollbar-thumb:vertical{min-height:42px;background:#2a2a2a url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_up_mid.png') repeat-y center top}.dropdown::-webkit-scrollbar-button:vertical:decrement{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_less_up.png') no-repeat center center}.dropdown::-webkit-scrollbar-button:vertical:increment{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_more_up.png') no-repeat center center}"
        ".shell{display:grid;grid-template-columns:196px 1fr;height:100%%;min-height:0;background:var(--nav)}.nav{background:var(--nav);padding:28px 20px 12px;border-right:1px solid #242424}.nav a{display:block;width:100%%;height:36px;margin-bottom:10px;line-height:36px;padding:0 14px;border:1px solid transparent;border-radius:4px;font-family:'Malgun Gothic';font-weight:800;letter-spacing:.03em}.nav a.active{background:var(--active);border-color:#343c40;border-left:5px solid var(--orange);padding-left:9px}"
        ".panel{margin:18px 16px 16px;background:var(--panel);border:1px solid var(--panel2);border-radius:5px;min-height:0;height:calc(100%% - 34px);display:grid;grid-template-rows:66px 1fr;overflow:hidden}.panelHead{background:var(--panel2);padding:10px 15px}.panelHead h1{margin:0;font-family:'Malgun Gothic';font-size:%dpx;font-weight:800}.panelHead p{margin:3px 0 0;color:var(--muted)}"
        ".content{padding:16px;min-height:0;height:100%%;display:flex;flex-direction:column;overflow:hidden}.content::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:20px}.content::-webkit-scrollbar:horizontal,.tablewrap::-webkit-scrollbar:horizontal,.card::-webkit-scrollbar:horizontal{display:none;height:0}.content::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_bar_mid.png') repeat-y center top}.content::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px;background:#2a2a2a url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_up_mid.png') repeat-y center top}.content::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_slider_over_mid.png')}.content::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_less_up.png') no-repeat center center}.content::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:20px;background:#101010 url('file:///C:/Program%%20Files%%20(x86)/Steam/steamapps/common/Out%%20of%%20the%%20Park%%20Baseball%%2027/data/skins/ootp%%20dark/style_sets/table_scrollbar/images/sb_more_up.png') no-repeat center center}.content::-webkit-scrollbar-corner,.tablewrap::-webkit-scrollbar-corner,.card::-webkit-scrollbar-corner{background:#101010}.rights,.card{flex:1;min-height:0;height:100%%}.rights{display:flex;flex-direction:column;gap:10px}.card{overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010;padding:16px}.reportbar{display:none}.muted{color:var(--muted);line-height:1.42}.help{display:flex;gap:8px;align-items:center;color:#b9b9b9}.help span{background:#242424;border:1px solid #343434;border-radius:4px;padding:4px 8px}.actions{display:flex;gap:8px;justify-content:flex-end}.action{display:inline-block;min-width:128px;text-align:center;color:#f4f4f4;border:1px solid #3a3a3a;border-radius:4px;background:#262626;font-family:'Malgun Gothic';font-weight:800;padding:8px 12px}.keep{background:#8d4b17;border-color:#c46b22}.release{background:#1d556c;border-color:#2e7896}.toggle{width:52px;text-align:center}.switch{display:inline-block;width:18px;height:18px;line-height:16px;margin-right:4px;text-align:center;color:#aaa;border:1px solid #343434;border-radius:3px;background:#181818;font-family:'Segoe UI Symbol','Malgun Gothic';font-size:11px;font-weight:400;text-decoration:none}.switch:hover{color:#f0f0f0;border-color:#696969;background:#242424}.switch.keep,.switch.release{background:#181818;border-color:#343434}.tablewrap{flex:1;min-height:0;overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010}table{width:100%%;table-layout:fixed;border-collapse:separate;border-spacing:0;font-family:'Malgun Gothic';font-size:13px}th{position:sticky;top:0;background:#222;color:#dedede;text-align:left;padding:8px 10px;border-bottom:1px solid #393939;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}td{padding:7px 10px;border-bottom:1px solid #242424;color:#d8d8d8;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}thead th:first-child{border-top-left-radius:3px}thead th:last-child{border-top-right-radius:3px}tbody tr:last-child td:first-child{border-bottom-left-radius:3px}tbody tr:last-child td:last-child{border-bottom-right-radius:3px}tr:nth-child(even) td{background:#151515}tr:hover td,tr.selected td{background:#243b45;color:#fff}.sel{width:48px;color:var(--orange);font-weight:900;white-space:nowrap}.pname{width:156px;max-width:156px;overflow:hidden;text-overflow:ellipsis;font-weight:800;color:#f3f0e8}.team{width:64px;max-width:64px;text-align:left;font-weight:800;color:#ddd}.flag{width:44px;font-family:'Segoe UI Emoji','Apple Color Emoji','Noto Color Emoji','Malgun Gothic';font-size:18px;line-height:1;text-align:center;cursor:help}.empty{border:1px dashed rgba(255,255,255,.22);border-radius:4px;padding:22px;color:var(--muted);background:#141414;white-space:normal}"
        ".app{height:100vh}.shell{height:calc(100vh - 64px);align-items:stretch}.panel{height:auto!important;min-height:0!important;margin:18px 16px 16px;align-self:stretch;grid-template-rows:66px minmax(0,1fr)}.content{height:auto!important;min-height:0;overflow-y:auto!important;overflow-x:hidden!important;scrollbar-gutter:stable}.rights{height:100%%!important;min-height:0}.tablewrap{flex:1 1 auto;min-height:0;scrollbar-gutter:stable}.card{height:100%%!important;min-height:0;scrollbar-gutter:stable}"
        "</style></head><body><div class='app'><header class='top'><div class='identity'>",
        g_kbo_hub_skin_article_font_px - 1,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_button_font_px - 4,
        g_kbo_hub_skin_article_font_px + 8,
        g_kbo_hub_skin_article_font_px - 2);
    if (league_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img class='logo' src='");
        kbo_webview_append_image_src(&buffer, league_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<div class='brandBlock'><div class='brand'>KBO FRONT OFFICE</div><div class='date'>");
    kbo_html_append_escaped(&buffer, current_date_text);
    kbo_window_text_appendf(&buffer, "</div></div></div><div class='selects'><a class='select' href='kbo://league'>");
    if (league_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img src='");
        kbo_webview_append_image_src(&buffer, league_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<span>");
    kbo_html_append_escaped(&buffer, league_name);
    kbo_window_text_appendf(&buffer, "</span><span>v</span></a><a class='select' href='kbo://team'>");
    if (team_logo_path[0] != '\0') {
        kbo_window_text_appendf(&buffer, "<img src='");
        kbo_webview_append_image_src(&buffer, team_logo_path);
        kbo_window_text_appendf(&buffer, "'>");
    }
    kbo_window_text_appendf(&buffer, "<span>");
    kbo_html_append_escaped(&buffer, team_name);
    kbo_window_text_appendf(&buffer, "</span><span>v</span></a></div></header><div class='shell'><nav class='nav'>");
    if (g_kbo_hub_open_dropdown == 1) {
        kbo_webview_append_league_dropdown(&buffer, current_year);
    } else if (g_kbo_hub_open_dropdown == 2) {
        kbo_webview_append_team_dropdown(&buffer, current_year);
    }

    for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
        kbo_window_text_appendf(&buffer, "<a class='%s' href='kbo://view/%d'>",
            i == g_kbo_hub_selected_view ? "active" : "", i);
        kbo_html_append_escaped(&buffer, kbo_hub_nav_label(i));
        kbo_window_text_appendf(&buffer, "</a>");
    }

    kbo_window_text_appendf(&buffer, "</nav><main class='panel'><div class='panelHead'><h1>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_title());
    kbo_window_text_appendf(&buffer, "</h1><p>");
    kbo_html_append_escaped(&buffer, kbo_hub_current_view_subtitle());
    kbo_window_text_appendf(&buffer, "</p></div><section class='content'>");

    kbo_webview_append_selected_view(&buffer, current_year, window_status);
    kbo_window_text_appendf(&buffer, "</section></main></div></div></body></html>");

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

