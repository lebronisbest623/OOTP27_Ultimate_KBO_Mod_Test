#ifndef KBOFIX_SRC_HOTKEY_WINDOW_HOTKEY_WINDOW_RUNTIME_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_HOTKEY_WINDOW_RUNTIME_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <tlhelp32.h>
#include <objbase.h>
#include <wincodec.h>
#include <shellapi.h>
#include <WebView2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/ootp_typedefs.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../build_verify/build_verify.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/dates/core_text_date.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/control/team_human_control.h"
#include "../../team/names/team_name_cache.h"
#include "../../team/names/team_string.h"
#include "../hotkey_window.h"
#include "../support/assets/paths/ui_asset_paths.h"
#include "../views/asian_games/ui_asian_games_view.h"
#include "../state/league_lookup/api/state_league_lookup.h"
#include "../state/team_vector/state_team_vector.h"
#include "../state/text/state_text_utils.h"
#include "../support/assets/names/support_names.h"
#include "../support/assets/logos/support_logos.h"
#include "../support/text/date/ui_date_format.h"
#include "../views/fa/ui_fa_views.h"
#include "../views/foreign/ui_foreign_rights_view.h"
#include "../support/assets/paths/ui_image_sources.h"
#include "../support/text/js/ui_js_string.h"
#include "../support/text/language/ui_language.h"
#include "../views/mod/info/ui_mod_info_views.h"
#include "../views/military/api/ui_military_view.h"
#include "../support/assets/nations/ui_nation_helpers.h"
#include "../support/roster/cells/ui_roster_cells.h"
#include "../views/mod/reputation/ui_reputation_view.h"
#include "../support/roster/sort/ui_roster_sort_script.h"
#include "../support/roster/table/ui_roster_table_css.h"
#include "../support/skin/ui_scrollbar_skin_css.h"
#include "../support/skin/ui_skin_metrics.h"
#include "../support/text/buffer/ui_text_buffer.h"
#include "../support/assets/names/ui_uniform_numbers.h"
#include "../support/skin_assets/bitmap_draw.h"
#include "../ui_html_helpers/position_helpers.h"

typedef struct KboMainWindowSearch {
    DWORD pid;
    HWND hwnd;
} KboMainWindowSearch;

typedef HRESULT (STDAPICALLTYPE *KboCreateCoreWebView2EnvironmentWithOptionsFn)(
    PCWSTR,
    PCWSTR,
    ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

typedef struct KboWebViewEnvHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewEnvHandler;

typedef struct KboWebViewControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewControllerHandler;

typedef struct KboWebViewNavHandler {
    ICoreWebView2NavigationStartingEventHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewNavHandler;

#define KBO_WM_TOGGLE_SERVICE_MONITOR (WM_APP + 0x4b0u)
#define KBO_HUB_CONTROL_EDIT            3
#define KBO_HUB_CONTROL_FOREIGN_LIST    41
#define KBO_HUB_CONTROL_FOREIGN_KEEP    42
#define KBO_HUB_CONTROL_FOREIGN_RELEASE 43
#define KBO_HUB_FIXED_CLIENT_WIDTH      1280
#define KBO_HUB_FIXED_CLIENT_HEIGHT     720
#define KBO_HUB_MIN_CLIENT_WIDTH        960
#define KBO_HUB_MIN_CLIENT_HEIGHT       560
#define KBO_HUB_VIEW_MOD_INFO    0
#define KBO_HUB_VIEW_MILITARY    1
#define KBO_HUB_VIEW_FOREIGN_RIGHTS 2
#define KBO_HUB_VIEW_ASIAN_QUOTA 3
#define KBO_HUB_VIEW_ASIAN_GAMES 4
#define KBO_HUB_VIEW_UPCOMING_FA 5
#define KBO_HUB_VIEW_FA_CASES   6
#define KBO_HUB_VIEW_SETTINGS    7
#define KBO_HUB_VIEW_REPUTATION  8
#define KBO_HUB_NAV_COUNT        9
#define KBO_HUB_FOREIGN_SUBVIEW_ROSTER 0
#define KBO_HUB_FOREIGN_SUBVIEW_RIGHTS 1
#define KBO_HUB_FOREIGN_SUBVIEW_COUNT  2
#define KBO_HUB_MOD_SUBVIEW_README        0
#define KBO_HUB_MOD_SUBVIEW_LICENSE       1
#define KBO_HUB_MOD_SUBVIEW_CREDITS       2
#define KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS 3
#define KBO_HUB_MOD_SUBVIEW_SETTINGS      4
#define KBO_HUB_MOD_SUBVIEW_COUNT         5

extern LONG g_kbo_hotkey_window_started;
extern HWND g_kbo_hotkey_window;
extern HWND g_kbo_hotkey_edit;
extern HWND g_kbo_foreign_list;
extern HWND g_kbo_foreign_keep_button;
extern HWND g_kbo_foreign_release_button;
extern ICoreWebView2Controller* g_kbo_webview_controller;
extern ICoreWebView2* g_kbo_webview;
extern LONG g_kbo_webview_starting;
extern LONG g_kbo_webview_ready;
extern LONG g_kbo_webview_failed;
extern DWORD g_kbo_hotkey_thread_id;
extern HINSTANCE g_kbo_hotkey_instance;
extern HHOOK g_kbo_hotkey_keyboard_hook;
extern WNDPROC g_kbo_hotkey_edit_original_proc;
extern ULONGLONG g_kbo_hotkey_last_toggle_ms;
extern HFONT g_kbo_hub_font_title;
extern HFONT g_kbo_hub_font_body;
extern HFONT g_kbo_hub_font_mono;
extern HFONT g_kbo_hub_font_small;
extern HBRUSH g_kbo_hub_brush_bg;
extern HBRUSH g_kbo_hub_brush_header;
extern HBRUSH g_kbo_hub_brush_panel;
extern HBRUSH g_kbo_hub_brush_panel_alt;
extern HBRUSH g_kbo_hub_brush_nav;
extern HBRUSH g_kbo_hub_brush_nav_active;
extern HBRUSH g_kbo_hub_brush_accent;
extern int g_kbo_hub_selected_view;
extern int g_kbo_hub_selected_mod_subview;
extern int g_kbo_hub_selected_foreign_subview;
extern int g_kbo_hub_selected_agames_subview;
extern int g_kbo_hub_selected_military_subview;
extern int g_kbo_hub_selected_fa_subview;
extern uint32_t g_kbo_hub_selected_fa_compensation_player_id;
extern uint32_t g_kbo_hub_selected_military_results_year;
extern RECT g_kbo_hub_refresh_rect;
extern RECT g_kbo_hub_language_rect;
extern RECT g_kbo_hub_github_rect;
extern RECT g_kbo_hub_lang_ko_rect;
extern RECT g_kbo_hub_lang_en_rect;
extern RECT g_kbo_hub_league_dropdown_rect;
extern RECT g_kbo_hub_team_dropdown_rect;
extern RECT g_kbo_hub_content_rect;
extern RECT g_kbo_hub_scrollbar_rect;
extern RECT g_kbo_hub_scrollbar_less_rect;
extern RECT g_kbo_hub_scrollbar_more_rect;
extern RECT g_kbo_hub_scrollbar_thumb_rect;
extern RECT g_kbo_hub_foreign_retain_rect;
extern RECT g_kbo_hub_foreign_skip_rect;
extern uint32_t g_kbo_hub_selected_league_id;
extern uint32_t g_kbo_hub_selected_team_id;
extern uint32_t g_kbo_hub_selected_foreign_player_id;
extern int g_kbo_hub_open_dropdown;
extern LONG g_kbo_hub_skin_assets_loaded;
extern HBITMAP g_kbo_hub_asset_github;
extern HBITMAP g_kbo_hub_asset_menu_arrow;
extern HBITMAP g_kbo_hub_asset_sb_bar_top;
extern HBITMAP g_kbo_hub_asset_sb_bar_mid;
extern HBITMAP g_kbo_hub_asset_sb_bar_bottom;
extern HBITMAP g_kbo_hub_asset_sb_less;
extern HBITMAP g_kbo_hub_asset_sb_more;
extern HBITMAP g_kbo_hub_asset_sb_slider_top;
extern HBITMAP g_kbo_hub_asset_sb_slider_mid;
extern HBITMAP g_kbo_hub_asset_sb_slider_bottom;
extern const COLORREF KBO_HUB_COLOR_BG;
extern const COLORREF KBO_HUB_COLOR_HEADER;
extern const COLORREF KBO_HUB_COLOR_NAV;
extern const COLORREF KBO_HUB_COLOR_NAV_ACTIVE;
extern const COLORREF KBO_HUB_COLOR_PANEL;
extern const COLORREF KBO_HUB_COLOR_PANEL_ALT;
extern const COLORREF KBO_HUB_COLOR_TEXT;
extern const COLORREF KBO_HUB_COLOR_MUTED;
extern const COLORREF KBO_HUB_COLOR_ACCENT;

int kbo_webview_handle_external_or_foreign_command(const char* cmd, HWND hwnd);
int kbo_webview_handle_view_navigation_command(const char* cmd);
int kbo_webview_handle_mod_settings_command(const char* cmd);
int kbo_webview_handle_event_and_fa_command(const char* cmd);
int kbo_webview_handle_settings_command(const char* cmd);
int kbo_webview_handle_selection_dropdown_command(const char* cmd, HWND hwnd);

const char* kbo_hub_nav_label(int index);
const char* kbo_hub_foreign_subnav_label(int index);
const char* kbo_hub_agames_subnav_label(int index);
const char* kbo_hub_fa_subnav_label(int index);
const char* kbo_hub_military_subnav_label(int index);
const char* kbo_hub_mod_subnav_label(int index);
void kbo_hub_ensure_valid_selection(void);
void kbo_refresh_hotkey_window(void);
void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position);

const char* kbo_hub_foreign_slot_code_for_player(uint8_t* player);
void kbo_hub_delete_bitmap(HBITMAP* bitmap);
HBITMAP kbo_hub_load_png_hbitmap_wic(const char* path);
void kbo_hub_load_skin_assets(void);
void kbo_hub_delete_skin_assets(void);
int kbo_hub_estimate_visible_edit_lines(void);
void kbo_hub_update_scrollbar_geometry(void);
void kbo_hub_draw_ootp_scrollbar(HDC hdc);
void kbo_hub_init_gdi_objects(void);
void kbo_hub_delete_gdi_objects(void);
BOOL CALLBACK kbo_enum_main_window_proc(HWND hwnd, LPARAM lparam);
HWND kbo_find_ootp_main_window(void);
int kbo_foreground_is_this_process(void);
void kbo_show_or_hide_hotkey_window(void);
int kbo_queue_hotkey_window_toggle(void);
LRESULT CALLBACK kbo_hotkey_keyboard_proc(int code, WPARAM wparam, LPARAM lparam);
int kbo_hub_argb_team_color_to_hex(uint32_t argb, char* out, size_t out_size);
int kbo_hub_copy_team_bar_colors(uint32_t team_id, char* primary, size_t primary_size, char* secondary, size_t secondary_size);
int kbo_hub_count_service_players(uint32_t service_team_id, int* out_due_60, int* out_due_now);
void kbo_build_overview_hub_text(char* out, size_t out_size);
void kbo_build_military_service_window_text(char* out, size_t out_size);
void kbo_build_foreign_rights_window_text(char* out, size_t out_size);
void kbo_build_foreign_injury_replacement_hub_text(char* out, size_t out_size);
void kbo_build_mod_info_hub_text(char* out, size_t out_size);
void kbo_build_foreign_policy_hub_text(char* out, size_t out_size);
void kbo_build_asian_games_hub_text(char* out, size_t out_size);
void kbo_build_fa_cases_hub_text(char* out, size_t out_size);
void kbo_build_fa_compensation_hub_text(char* out, size_t out_size);
void kbo_build_settings_hub_text(char* out, size_t out_size);
void kbo_build_hub_window_text(char* out, size_t out_size);
void kbo_refresh_hotkey_window(void);
RECT kbo_hub_nav_item_rect(int index, int width, int height);
const char* kbo_hub_current_view_title(void);
const char* kbo_hub_current_view_subtitle(void);
int kbo_hub_league_keeps_allstar_teams(uint32_t league_id);
int kbo_hub_team_name_is_allstar(const char* name);
int kbo_hub_team_hidden_from_dropdown(uint8_t* team);
void kbo_hub_ensure_valid_selection(void);
POINT kbo_hub_dropdown_anchor_point(HWND hwnd, const RECT* rect);
void kbo_hub_show_league_dropdown(HWND hwnd);
void kbo_hub_show_team_dropdown(HWND hwnd);
int kbo_hub_foreign_rights_ui_selected(void);
uint32_t kbo_parse_foreign_candidate_player_id_from_line(const char* line);
int kbo_select_foreign_candidate_from_edit_click(HWND edit, LPARAM lparam);
LRESULT CALLBACK kbo_hotkey_edit_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
void kbo_refresh_foreign_rights_controls(void);
void kbo_apply_foreign_rights_button(int retain);
void kbo_webview_append_league_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year);
void kbo_webview_append_team_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year);
void kbo_webview_append_asian_quota_view(KboWindowTextBuffer* buffer);
void kbo_webview_append_fallback_text_view(KboWindowTextBuffer* buffer);
void kbo_webview_append_selected_view(KboWindowTextBuffer* buffer, uint32_t current_year, const char* window_status);
int kbo_hub_selected_league_is_amateur_reputation_league(void);
int kbo_hub_selected_league_is_kbo(void);
int kbo_hub_view_available_for_selected_league(int view);
void kbo_webview_append_main_tabs(KboWindowTextBuffer* buffer);
void kbo_webview_append_sub_tabs(KboWindowTextBuffer* buffer);
int kbo_webview_current_view_has_sub_tabs(void);
WCHAR* kbo_build_webview_hub_html(void);
void kbo_webview_navigate_current(void);
int kbo_webview_team_action_allowed(uint32_t team_id, const char* source);
HRESULT STDMETHODCALLTYPE kbo_webview_nav_qi(ICoreWebView2NavigationStartingEventHandler* This, REFIID riid, void** ppv);
ULONG STDMETHODCALLTYPE kbo_webview_nav_addref(ICoreWebView2NavigationStartingEventHandler* This);
ULONG STDMETHODCALLTYPE kbo_webview_nav_release(ICoreWebView2NavigationStartingEventHandler* This);
int kbo_webview_handle_command_uri(const char* uri, HWND hwnd);
HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args);
HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv);
ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This);
ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This);
HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv);
ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This);
ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This);
void kbo_webview_set_bounds(HWND hwnd);
void kbo_webview_apply_ootp_like_settings(void);
HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result);
HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Environment* result);
void kbo_start_webview_rights_ui(HWND hwnd);
void kbo_layout_hotkey_window(HWND hwnd);
void kbo_refresh_hotkey_window_layout(HWND hwnd);
void kbo_hub_get_work_area(HWND hwnd, RECT* out);
RECT kbo_hub_fixed_window_rect(HWND hwnd);
void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position);
LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter);
void start_kbo_hotkey_window_thread(HINSTANCE instance);

#endif
