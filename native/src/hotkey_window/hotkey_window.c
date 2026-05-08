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

#include "../bootstrap/ootp_offsets.h"
#include "../bootstrap/ootp_typedefs.h"
#include "../bootstrap/profiler.h"
#include "../build_verify/build_verify.h"
#include "../amateur_player_quality/amateur_player_quality.h"
#include "../core/core_atomic_file.h"
#include "../core/core_current_date.h"
#include "../core/core_flags/flags_api.h"
#include "../core/core_flags/localappdata_reader.h"
#include "../core/core_league_context_parts/event_manager.h"
#include "../core/core_league_context_parts/league_context_lookup.h"
#include "../core/core_live_news.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../core/core_text_date.h"
#include "../custom_events/asian_games_state.h"
#include "../custom_events/asian_games_schedule_seed/state_and_paths.h"
#include "../custom_events/asian_games_lifecycle_departure.h"
#include "../custom_events/asian_games_lifecycle_final.h"
#include "../custom_events/asian_games_lifecycle_replacement.h"
#include "../custom_events/asian_games_lifecycle_roster.h"
#include "../custom_events/asian_games_news/body.h"
#include "../custom_events/asian_games_news/emit.h"
#include "../custom_events/asian_games_news/handlers.h"
#include "../custom_events/asian_games_news/links.h"
#include "../custom_events/asian_games_player_eval.h"
#include "../custom_events/asian_games_roster_store.h"
#include "../custom_events/asian_games_schedule.h"
#include "../custom_events/asian_games_schedule_seed/import_and_load.h"
#include "../custom_events/asian_games_schedule_seed/query_helpers.h"
#include "../custom_events/asian_games_schedule_seed/state_and_paths.h"
#include "../custom_events/asian_games_selection/missing_org.h"
#include "../custom_events/asian_games_selection/select_roster.h"
#include "../custom_events/asian_games_selection/selection_pick.h"
#include "../custom_events/asian_games_selection/wildcards.h"
#include "../custom_events/asian_games_state.h"
#include "../custom_events/custom_event_dispatch.h"
#include "../custom_events/custom_event_lookup.h"
#include "../custom_events/custom_event_markers.h"
#include "../custom_events/custom_event_names.h"
#include "../custom_events/custom_event_state.h"
#include "../custom_events/custom_events_common.h"
#include "../custom_events/foreign_priority_event_schedule.h"
#include "../custom_events/offseason_transition_schedule.h"
#include "../fa_compensation/fa_compensation_decisions.h"
#include "../fa_compensation/fa_compensation_due.h"
#include "../fa_compensation/fa_compensation_history.h"
#include "../fa_compensation/fa_compensation_market.h"
#include "../fa_compensation/fa_compensation_news_transfer.h"
#include "../fa_compensation/fa_compensation_protected_lists.h"
#include "../fa_compensation/fa_compensation_protection_score.h"
#include "../fa_compensation/fa_compensation_records.h"
#include "../fa_compensation/fa_compensation_selection.h"
#include "../fa_compensation/fa_compensation_state.h"
#include "../fa_filing/fa_filing.h"
#include "../fa_market_classification/fa_market_classification.h"
#include "../fa_rules/fa_rules.h"
#include "../foreign/foreign_waiver_config.h"
#include "../foreign/replacement_seed/foreign_replacement_seed.h"
#include "../foreign/foreign_waiver_core.h"
#include "../foreign/foreign_waiver_date.h"
#include "../foreign/foreign_waiver_player_eval.h"
#include "../foreign/foreign_waiver_policy.h"
#include "../foreign/injury/foreign_injury.h"
#include "../foreign/rights/foreign_waiver_rights_query.h"
#include "../military_service/military_service.h"
#include "../military_service/military_player_state.h"
#include "../military_service/military_selection_event.h"
#include "../military_service/military_service_team_policy.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_lookup.h"
#include "../team/team_org_assignment_query.h"
#include "../team/team_human_control.h"
#include "../team/team_name_cache.h"
#include "../team/team_string.h"
#include "hotkey_window.h"
#include "state_team_vector.h"
#include "state_text_utils.h"
#include "ui_text_buffer.h"
#include "support/skin_assets/bitmap_draw.h"
#include "ui_html_helpers/position_helpers.h"

/* ---- native\src\hotkey_window\state.inc ---- */
/* F2 hub state assembler. */

/* ---- native\src\hotkey_window\state_types.inc ---- */

typedef struct KboMainWindowSearch {
    DWORD pid;
    HWND hwnd;
} KboMainWindowSearch;
/* ---- native\src\hotkey_window\state_window.inc ---- */
static LONG g_kbo_hotkey_window_started = 0;
static HWND g_kbo_hotkey_window = NULL;
static HWND g_kbo_hotkey_edit = NULL;
static HWND g_kbo_foreign_list = NULL;
static HWND g_kbo_foreign_keep_button = NULL;
static HWND g_kbo_foreign_release_button = NULL;
static ICoreWebView2Controller* g_kbo_webview_controller = NULL;
static ICoreWebView2* g_kbo_webview = NULL;
static LONG g_kbo_webview_starting = 0;
static LONG g_kbo_webview_ready = 0;
static LONG g_kbo_webview_failed = 0;
static DWORD g_kbo_hotkey_thread_id = 0;
static HINSTANCE g_kbo_hotkey_instance = NULL;
static HHOOK g_kbo_hotkey_keyboard_hook = NULL;
static WNDPROC g_kbo_hotkey_edit_original_proc = NULL;
static ULONGLONG g_kbo_hotkey_last_toggle_ms = 0;
/* ---- native\src\hotkey_window\state_gdi.inc ---- */
static HFONT g_kbo_hub_font_title = NULL;
static HFONT g_kbo_hub_font_body = NULL;
static HFONT g_kbo_hub_font_mono = NULL;
static HFONT g_kbo_hub_font_small = NULL;
static HBRUSH g_kbo_hub_brush_bg = NULL;
static HBRUSH g_kbo_hub_brush_header = NULL;
static HBRUSH g_kbo_hub_brush_panel = NULL;
static HBRUSH g_kbo_hub_brush_panel_alt = NULL;
static HBRUSH g_kbo_hub_brush_nav = NULL;
static HBRUSH g_kbo_hub_brush_nav_active = NULL;
static HBRUSH g_kbo_hub_brush_accent = NULL;
/* ---- native\src\hotkey_window\state_view.inc ---- */
static int g_kbo_hub_selected_view = 0;
static int g_kbo_hub_selected_mod_subview = 0;
static int g_kbo_hub_selected_foreign_subview = 0;
static int g_kbo_hub_selected_agames_subview = 0;
static int g_kbo_hub_selected_military_subview = 0;
static int g_kbo_hub_selected_fa_subview = 0;
static uint32_t g_kbo_hub_selected_fa_compensation_player_id = 0u;
static uint32_t g_kbo_hub_selected_military_results_year = 0;
static int g_kbo_hub_language = 0;
static RECT g_kbo_hub_refresh_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_language_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_github_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_lang_ko_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_lang_en_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_league_dropdown_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_team_dropdown_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_content_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_scrollbar_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_scrollbar_less_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_scrollbar_more_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_scrollbar_thumb_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_foreign_retain_rect = {0, 0, 0, 0};
static RECT g_kbo_hub_foreign_skip_rect = {0, 0, 0, 0};
static uint32_t g_kbo_hub_selected_league_id = 0;
static uint32_t g_kbo_hub_selected_team_id = 0;
static uint32_t g_kbo_hub_selected_foreign_player_id = 0;
static int g_kbo_hub_open_dropdown = 0;
/* ---- native\src\hotkey_window\state_skin.inc ---- */
static LONG g_kbo_hub_skin_metrics_loaded = 0;
static LONG g_kbo_hub_skin_assets_loaded = 0;
static int g_kbo_hub_skin_article_gap_x = 16;
static int g_kbo_hub_skin_article_gap_y = 8;
static int g_kbo_hub_skin_article_font_px = 16;
static int g_kbo_hub_skin_button_font_px = 16;
static int g_kbo_hub_skin_scrollbar_width = 20;
static HBITMAP g_kbo_hub_asset_github = NULL;
static HBITMAP g_kbo_hub_asset_menu_arrow = NULL;
static HBITMAP g_kbo_hub_asset_sb_bar_top = NULL;
static HBITMAP g_kbo_hub_asset_sb_bar_mid = NULL;
static HBITMAP g_kbo_hub_asset_sb_bar_bottom = NULL;
static HBITMAP g_kbo_hub_asset_sb_less = NULL;
static HBITMAP g_kbo_hub_asset_sb_more = NULL;
static HBITMAP g_kbo_hub_asset_sb_slider_top = NULL;
static HBITMAP g_kbo_hub_asset_sb_slider_mid = NULL;
static HBITMAP g_kbo_hub_asset_sb_slider_bottom = NULL;
static HBITMAP g_kbo_hub_asset_selected_league_logo = NULL;
static HBITMAP g_kbo_hub_asset_selected_team_logo = NULL;
static uint32_t g_kbo_hub_logo_cache_league_id = 0;
static uint32_t g_kbo_hub_logo_cache_team_id = 0;
static uint32_t g_kbo_hub_logo_cache_year = 0;
/* ---- native\src\hotkey_window\state_constants.inc ---- */
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
#define KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS 0
#define KBO_HUB_AGAMES_SUBVIEW_SCHEDULE    1
#define KBO_HUB_AGAMES_SUBVIEW_ROSTER      2
#define KBO_HUB_AGAMES_SUBVIEW_COUNT       3
#define KBO_HUB_FA_SUBVIEW_MARKET          0
#define KBO_HUB_FA_SUBVIEW_COMPENSATION    1
#define KBO_HUB_FA_SUBVIEW_COUNT           2
#define KBO_HUB_MILITARY_SUBVIEW_ROSTER     0
#define KBO_HUB_MILITARY_SUBVIEW_APPLICANTS 1
#define KBO_HUB_MILITARY_SUBVIEW_RESULTS    2
#define KBO_HUB_MILITARY_SUBVIEW_COUNT      3
#define KBO_HUB_LANG_KO 0
#define KBO_HUB_LANG_EN 1

static const COLORREF KBO_HUB_COLOR_BG         = RGB(0x0A, 0x0A, 0x0A);
static const COLORREF KBO_HUB_COLOR_HEADER      = RGB(0x1D, 0x55, 0x6C);
static const COLORREF KBO_HUB_COLOR_NAV         = RGB(0x14, 0x14, 0x14);
static const COLORREF KBO_HUB_COLOR_NAV_ACTIVE  = RGB(0x1D, 0x55, 0x6C);
static const COLORREF KBO_HUB_COLOR_PANEL       = RGB(0x16, 0x16, 0x16);
static const COLORREF KBO_HUB_COLOR_PANEL_ALT   = RGB(0x22, 0x22, 0x22);
static const COLORREF KBO_HUB_COLOR_TEXT        = RGB(0xFC, 0xFC, 0xFC);
static const COLORREF KBO_HUB_COLOR_MUTED       = RGB(0x8F, 0x8F, 0x8F);
static const COLORREF KBO_HUB_COLOR_ACCENT      = RGB(0xDE, 0x6D, 0x1F);
static const char* kbo_hub_text(const char* ko, const char* en)
{
    return g_kbo_hub_language == KBO_HUB_LANG_EN ? en : ko;
}
#include "state_team_vector.h"
/* ---- native\src\hotkey_window\state_language.inc ---- */
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

static void kbo_hub_load_language_setting(void)
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

static void kbo_hub_save_language_setting(void)
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
/* ---- native\src\hotkey_window\state_nav.inc ---- */
static const char* kbo_hub_nav_label(int index)
{
    switch (index) {
    case KBO_HUB_VIEW_MOD_INFO:       return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_MILITARY:       return kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80",  "SERVICE TEAMS");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c",  "RIGHTS");
    case KBO_HUB_VIEW_ASIAN_QUOTA:    return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "FOREIGN PLAYERS");
    case KBO_HUB_VIEW_ASIAN_GAMES:    return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84", "AGAMES");
    case KBO_HUB_VIEW_UPCOMING_FA:    return "";
    case KBO_HUB_VIEW_FA_CASES:       return kbo_hub_text("FA", "FA");
    case KBO_HUB_VIEW_SETTINGS:       return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",    "SETTINGS");
    case KBO_HUB_VIEW_REPUTATION:     return kbo_hub_text("\xed\x8f\x89\xed\x8c\x90", "REPUTATION");
    default:                          return "";
    }
}

static const char* kbo_hub_foreign_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FOREIGN_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "ROSTER");
    case KBO_HUB_FOREIGN_SUBVIEW_RIGHTS:
        return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "RIGHTS");
    default:
        return "";
    }
}

static const char* kbo_hub_agames_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS:
        return kbo_hub_text("\xeb\x8c\x80\xed\x9a\x8c", "TOURNAMENTS");
    case KBO_HUB_AGAMES_SUBVIEW_SCHEDULE:
        return kbo_hub_text("\xec\x9d\xbc\xec\xa0\x95", "SCHEDULE");
    case KBO_HUB_AGAMES_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "ROSTER");
    default:
        return "";
    }
}

static const char* kbo_hub_fa_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FA_SUBVIEW_MARKET:
        return kbo_hub_text("\xec\x8b\x9c\xec\x9e\xa5", "MARKET");
    case KBO_HUB_FA_SUBVIEW_COMPENSATION:
        return kbo_hub_text("\xeb\xb3\xb4\xec\x83\x81", "COMPENSATION");
    default:
        return "";
    }
}

static const char* kbo_hub_military_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_MILITARY_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "ROSTER");
    case KBO_HUB_MILITARY_SUBVIEW_APPLICANTS:
        return kbo_hub_text("\xec\xa7\x80\xec\x9b\x90\xec\x9e\x90", "APPLICANTS");
    case KBO_HUB_MILITARY_SUBVIEW_RESULTS:
        return kbo_hub_text("\xeb\xb0\x9c\xed\x91\x9c \xea\xb2\xb0\xea\xb3\xbc", "RESULTS");
    default:
        return "";
    }
}

static const char* kbo_hub_mod_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_MOD_SUBVIEW_README:
        return kbo_hub_text("\xeb\xa6\xac\xeb\x93\x9c\xeb\xaf\xb8", "README");
    case KBO_HUB_MOD_SUBVIEW_LICENSE:
        return kbo_hub_text("\xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4", "LICENSE");
    case KBO_HUB_MOD_SUBVIEW_CREDITS:
        return kbo_hub_text("\xed\x81\xac\xeb\xa0\x88\xeb\x94\xa7", "CREDITS");
    case KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS:
        return kbo_hub_text("\xea\xb8\xb0\xec\x97\xac", "CONTRIBUTIONS");
    case KBO_HUB_MOD_SUBVIEW_SETTINGS:
        return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "SETTINGS");
    default:
        return "";
    }
}
#include "state_text_utils.h"
/* ---- native\src\hotkey_window\state_league_lookup\league_lookup_state.inc ---- */
static uint32_t  g_kbo_league_ptr_cache_id  = 0;
static uintptr_t g_kbo_league_ptr_cache_ptr = 0;
#define KBO_LEAGUE_PTR_MISS_CACHE_MAX 32
static uint32_t  g_kbo_league_ptr_miss_cache_ids[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};
static ULONGLONG g_kbo_league_ptr_miss_cache_until_ms[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};

#define KBO_NAMED_LEAGUE_SCAN_MIN_SCORE 90
#define KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE 115
#define KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN (OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
#define KBO_LEAGUE_DISPLAY_CACHE_MAX 64

typedef struct KboHubLeagueDisplayCacheEntry {
    uint32_t league_id;
    uintptr_t league_ptr;
    uint32_t year;
    int score;
    char name[96];
    char logo_file[128];
} KboHubLeagueDisplayCacheEntry;

static uintptr_t g_kbo_league_display_cache_global = 0;
static uintptr_t g_kbo_league_display_cache_prewarmed_global = 0;
static KboHubLeagueDisplayCacheEntry g_kbo_league_display_cache[KBO_LEAGUE_DISPLAY_CACHE_MAX];

/* ---- native\src\hotkey_window\state_league_lookup\league_display_cache.inc ---- */
static void kbo_hub_clear_league_display_cache(void)
{
    memset(g_kbo_league_display_cache, 0, sizeof(g_kbo_league_display_cache));
    memset(g_kbo_league_ptr_miss_cache_ids, 0, sizeof(g_kbo_league_ptr_miss_cache_ids));
    memset(g_kbo_league_ptr_miss_cache_until_ms, 0, sizeof(g_kbo_league_ptr_miss_cache_until_ms));
    g_kbo_league_ptr_cache_id = 0;
    g_kbo_league_ptr_cache_ptr = 0;
    g_kbo_league_display_cache_prewarmed_global = 0;
}

static void kbo_hub_refresh_league_cache_context(void)
{
    uintptr_t global = get_ootp_global_database();
    if (global != g_kbo_league_display_cache_global) {
        g_kbo_league_display_cache_global = global;
        kbo_hub_clear_league_display_cache();
    }
}

static int kbo_hub_find_league_display_cache_slot(uint32_t league_id)
{
    for (int i = 0; i < KBO_LEAGUE_DISPLAY_CACHE_MAX; i++) {
        if (g_kbo_league_display_cache[i].league_id == league_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_find_league_display_cache_insert_slot(uint32_t league_id)
{
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot >= 0) {
        return slot;
    }

    for (int i = 0; i < KBO_LEAGUE_DISPLAY_CACHE_MAX; i++) {
        if (g_kbo_league_display_cache[i].league_id == 0u) {
            return i;
        }
    }

    return (int)(league_id % KBO_LEAGUE_DISPLAY_CACHE_MAX);
}

static void kbo_hub_store_league_display_cache(
    uint32_t league_id,
    uintptr_t league_ptr,
    int score,
    const char* name,
    const char* logo_file)
{
    if (league_id == 0 || league_ptr == 0 || name == NULL || name[0] == '\0') {
        return;
    }

    int slot = kbo_hub_find_league_display_cache_insert_slot(league_id);
    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->league_id == league_id
            && entry->name[0] != '\0'
            && entry->score > score) {
        return;
    }

    entry->league_id = league_id;
    entry->league_ptr = league_ptr;
    entry->score = score;
    entry->year = 0;
    entry->logo_file[0] = '\0';
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        entry->year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    if (logo_file != NULL && logo_file[0] != '\0') {
        snprintf(entry->logo_file, sizeof(entry->logo_file), "%s", logo_file);
    }
}

static int kbo_hub_try_copy_cached_league_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->name[0] == '\0' || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        return 0;
    }

    snprintf(out, out_size, "%s", entry->name);
    return 1;
}

static int kbo_hub_try_copy_cached_league_logo_file(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->logo_file[0] == '\0' || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        return 0;
    }

    snprintf(out, out_size, "%s", entry->logo_file);
    return 1;
}

/* ---- native\src\hotkey_window\state_league_lookup\league_ptr_cache.inc ---- */
static int kbo_hub_try_get_cached_league_ptr(uint32_t league_id, uintptr_t* out_league_ptr)
{
    if (out_league_ptr != NULL) {
        *out_league_ptr = 0;
    }

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->league_ptr == 0
            || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE
            || !memory_range_readable((void*)entry->league_ptr, KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
        memset(entry, 0, sizeof(*entry));
        return 0;
    }

    if (out_league_ptr != NULL) {
        *out_league_ptr = entry->league_ptr;
    }
    return 1;
}

static int kbo_league_ptr_recent_miss(uint32_t league_id, ULONGLONG now_ms)
{
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id
                && now_ms < g_kbo_league_ptr_miss_cache_until_ms[i]) {
            return 1;
        }
    }
    return 0;
}

static void kbo_remember_league_ptr_miss(uint32_t league_id, ULONGLONG now_ms)
{
    int slot = -1;
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id) {
            slot = i;
            break;
        }
        if (slot < 0 && (g_kbo_league_ptr_miss_cache_ids[i] == 0u
                || now_ms >= g_kbo_league_ptr_miss_cache_until_ms[i])) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = (int)(league_id % KBO_LEAGUE_PTR_MISS_CACHE_MAX);
    }
    g_kbo_league_ptr_miss_cache_ids[slot] = league_id;
    g_kbo_league_ptr_miss_cache_until_ms[slot] = now_ms + 3000u;
}

static void kbo_forget_league_ptr_miss(uint32_t league_id)
{
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id) {
            g_kbo_league_ptr_miss_cache_ids[i] = 0;
            g_kbo_league_ptr_miss_cache_until_ms[i] = 0;
        }
    }
}

/* ---- native\src\hotkey_window\state_league_lookup\league_name_score.inc ---- */
static int kbo_hub_text_ends_with_ignore_case(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }

    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len == 0 || text_len < suffix_len) {
        return 0;
    }

    return ascii_equals_ignore_case(text + text_len - suffix_len, suffix);
}

static int kbo_hub_league_name_likeness_score(const char* name)
{
    if (name == NULL) {
        return -100;
    }

    size_t len = strlen(name);
    if (len < 3u || len > 80u) {
        return -100;
    }

    int letters = 0;
    int digits = 0;
    int spaces = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            letters++;
        } else if (c >= '0' && c <= '9') {
            digits++;
        } else if (c == ' ') {
            spaces++;
        } else if (c != '-' && c != '_' && c != '.' && c != '&' && c != '\'') {
            return -50;
        }
    }

    if (letters < 3 || digits > letters / 2) {
        return -50;
    }

    int score = 0;
    if (spaces > 0) {
        score += 12;
    }
    if (len >= 12u) {
        score += 8;
    }
    if (kbo_ascii_contains_ignore_case(name, "League")
            || kbo_ascii_contains_ignore_case(name, "Organization")
            || kbo_ascii_contains_ignore_case(name, "Association")
            || kbo_ascii_contains_ignore_case(name, "Federation")
            || kbo_ascii_contains_ignore_case(name, "Baseball")
            || kbo_ascii_contains_ignore_case(name, "Conference")
            || kbo_ascii_contains_ignore_case(name, "Division")) {
        score += 30;
    }

    return score;
}

static int kbo_hub_abbr_likeness_score(const char* abbr)
{
    if (abbr == NULL) {
        return 0;
    }

    size_t len = strlen(abbr);
    if (len < 2u || len > 12u) {
        return 0;
    }

    int strong = 1;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)abbr[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            strong = 0;
            break;
        }
    }

    return strong ? 15 : 8;
}

static int kbo_hub_named_league_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    char* out_name,
    size_t out_name_size,
    char* out_logo_file,
    size_t out_logo_file_size)
{
    if (out_name != NULL && out_name_size > 0) {
        out_name[0] = '\0';
    }
    if (out_logo_file != NULL && out_logo_file_size > 0) {
        out_logo_file[0] = '\0';
    }
    if (candidate == 0 || league_id == 0
            || !memory_range_readable((void*)candidate, KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
        return -1000;
    }

    uint32_t primary_id = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
    uint32_t alternate_id = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    if (primary_id != league_id && alternate_id != league_id) {
        return -1000;
    }

    uint32_t year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < 1982u || year > 2200u) {
        return -200;
    }

    char name[96] = {0};
    if (!copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, name, sizeof(name))) {
        return -200;
    }

    int name_score = kbo_hub_league_name_likeness_score(name);
    if (name_score < 30) {
        return -200;
    }

    int score = 0;
    score += (primary_id == league_id) ? 30 : 20;
    score += 20;
    score += name_score;

    uint32_t subleague_count = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_SUBLEAGUE_COUNT_OFFSET);
    if (subleague_count <= 32u) {
        score += 5;
    }

    char abbr[32] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_ABBR_STRING_OFFSET, abbr, sizeof(abbr))) {
        score += kbo_hub_abbr_likeness_score(abbr);
    }

    char logo[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_LOGO_STRING_OFFSET, logo, sizeof(logo))
            && (kbo_hub_text_ends_with_ignore_case(logo, ".png")
                || kbo_hub_text_ends_with_ignore_case(logo, ".oi")
                || kbo_hub_text_ends_with_ignore_case(logo, ".jpg")
                || kbo_hub_text_ends_with_ignore_case(logo, ".jpeg"))) {
        score += 6;
        if (out_logo_file != NULL && out_logo_file_size > 0) {
            snprintf(out_logo_file, out_logo_file_size, "%s", logo);
        }
    }

    char stats_path[160] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_STATS_PATH_STRING_OFFSET, stats_path, sizeof(stats_path))
            && kbo_ascii_contains_ignore_case(stats_path, "\\data\\stats\\")) {
        score += 4;
    }

    char schedule_file[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET, schedule_file, sizeof(schedule_file))
            && kbo_hub_text_ends_with_ignore_case(schedule_file, ".lsdl")) {
        score += 6;
    }

    if (out_name != NULL && out_name_size > 0) {
        snprintf(out_name, out_name_size, "%s", name);
    }
    return score;
}

/* ---- native\src\hotkey_window\state_league_lookup\league_memory_scan.inc ---- */
static uintptr_t kbo_scan_named_league_ptr(uint32_t league_id, SIZE_T max_region_size, int* out_score, char* out_name, size_t out_name_size)
{
    uintptr_t best_ptr = 0;
    int best_score = -1000;
    char best_name[96] = {0};
    char best_logo[128] = {0};

    uintptr_t address = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                if (*(uint32_t*)p != league_id) {
                    continue;
                }

                static const uint32_t id_offsets[] = {
                    OOTP27_KBO_LEAGUE_ID_OFFSET,
                    OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
                };
                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0
                            || candidate < base
                            || candidate + KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN > end) {
                        continue;
                    }

                    char candidate_name[96] = {0};
                    char candidate_logo[128] = {0};
                    int score = kbo_hub_named_league_candidate_score(
                        candidate,
                        league_id,
                        candidate_name,
                        sizeof(candidate_name),
                        candidate_logo,
                        sizeof(candidate_logo));
                    if (score > best_score) {
                        best_score = score;
                        best_ptr = candidate;
                        snprintf(best_name, sizeof(best_name), "%s", candidate_name);
                        snprintf(best_logo, sizeof(best_logo), "%s", candidate_logo);
                        if (score >= KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE) {
                            if (out_score != NULL) {
                                *out_score = best_score;
                            }
                            if (out_name != NULL && out_name_size > 0) {
                                snprintf(out_name, out_name_size, "%s", best_name);
                            }
                            kbo_hub_store_league_display_cache(league_id, best_ptr, best_score, best_name, best_logo);
                            return best_ptr;
                        }
                    }
                }
            }
        }

        address = end;
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
    }

    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_name != NULL && out_name_size > 0) {
        snprintf(out_name, out_name_size, "%s", best_name);
    }
    if (best_ptr != 0 && best_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        kbo_hub_store_league_display_cache(league_id, best_ptr, best_score, best_name, best_logo);
    }
    return best_ptr;
}

/* ---- native\src\hotkey_window\state_league_lookup\league_visible_ids.inc ---- */
static int kbo_hub_league_id_list_index(const uint32_t* league_ids, int league_count, uint32_t league_id)
{
    if (league_ids == NULL || league_id == 0) {
        return -1;
    }
    for (int i = 0; i < league_count; i++) {
        if (league_ids[i] == league_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_collect_visible_league_ids(uint32_t* league_ids, int max_leagues)
{
    if (league_ids == NULL || max_leagues <= 0) {
        return 0;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return 0;
    }

    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < max_leagues; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || kbo_hub_league_id_list_index(league_ids, league_count, league_id) >= 0) {
            continue;
        }

        league_ids[league_count++] = league_id;
    }

    return league_count;
}

static int kbo_hub_count_cached_league_ids(const uint32_t* league_ids, int league_count)
{
    int count = 0;
    for (int i = 0; i < league_count; i++) {
        if (kbo_hub_find_league_display_cache_slot(league_ids[i]) >= 0) {
            count++;
        }
    }
    return count;
}

/* ---- native\src\hotkey_window\state_league_lookup\league_multi_scan.inc ---- */
static int kbo_scan_named_league_ptrs_for_ids(const uint32_t* league_ids, int league_count, SIZE_T max_region_size)
{
    if (league_ids == NULL || league_count <= 0) {
        return 0;
    }

    int found = kbo_hub_count_cached_league_ids(league_ids, league_count);
    uintptr_t address = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                uint32_t probe_id = *(uint32_t*)p;
                if (kbo_hub_league_id_list_index(league_ids, league_count, probe_id) < 0) {
                    continue;
                }

                static const uint32_t id_offsets[] = {
                    OOTP27_KBO_LEAGUE_ID_OFFSET,
                    OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
                };
                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0
                            || candidate < base
                            || candidate + KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN > end) {
                        continue;
                    }

                    char candidate_name[96] = {0};
                    char candidate_logo[128] = {0};
                    int score = kbo_hub_named_league_candidate_score(
                        candidate,
                        probe_id,
                        candidate_name,
                        sizeof(candidate_name),
                        candidate_logo,
                        sizeof(candidate_logo));
                    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
                        continue;
                    }

                    int before = kbo_hub_find_league_display_cache_slot(probe_id) >= 0;
                    kbo_hub_store_league_display_cache(probe_id, candidate, score, candidate_name, candidate_logo);
                    if (!before && kbo_hub_find_league_display_cache_slot(probe_id) >= 0) {
                        found++;
                        if (found >= league_count) {
                            return found;
                        }
                    }
                }
            }
        }

        address = end;
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
    }

    return found;
}

/* ---- native\src\hotkey_window\state_league_lookup\league_prewarm.inc ---- */
static void kbo_hub_prewarm_league_display_cache(void)
{
    kbo_hub_refresh_league_cache_context();
    uintptr_t global = g_kbo_league_display_cache_global;
    if (global == 0 || g_kbo_league_display_cache_prewarmed_global == global) {
        return;
    }

    uint32_t league_ids[KBO_LEAGUE_DISPLAY_CACHE_MAX] = {0};
    int league_count = kbo_hub_collect_visible_league_ids(league_ids, KBO_LEAGUE_DISPLAY_CACHE_MAX);
    if (league_count <= 0) {
        return;
    }

    ULONGLONG started_ms = GetTickCount64();
    int found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00040000u);
    if (found < league_count) {
        found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00400000u);
    }
    g_kbo_league_display_cache_prewarmed_global = global;
    append_logf(
        "KBO: F2 league cache prewarmed leagues=%d found=%d ms=%llu",
        league_count,
        found,
        (unsigned long long)(GetTickCount64() - started_ms));
}

/* ---- native\src\hotkey_window\state_league_lookup\league_find_ptr.inc ---- */
static uintptr_t kbo_find_league_ptr(uint32_t league_id)
{
    if (league_id == 0) {
        return 0;
    }

    kbo_hub_refresh_league_cache_context();

    ULONGLONG now_ms = GetTickCount64();
    if (kbo_league_ptr_recent_miss(league_id, now_ms)) {
        return 0;
    }

    uintptr_t cached_league_ptr = 0;
    if (kbo_hub_try_get_cached_league_ptr(league_id, &cached_league_ptr)) {
        return cached_league_ptr;
    }

    if (g_kbo_league_ptr_cache_id == league_id && g_kbo_league_ptr_cache_ptr != 0) {
        int cached_score = kbo_hub_named_league_candidate_score(
            g_kbo_league_ptr_cache_ptr,
            league_id,
            NULL,
            0,
            NULL,
            0);
        if (cached_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
            return g_kbo_league_ptr_cache_ptr;
        }
        g_kbo_league_ptr_cache_id  = 0;
        g_kbo_league_ptr_cache_ptr = 0;
    }

    append_logf("KBO: named league ptr scan started id=%u", league_id);

    int score = -1000;
    char name[96] = {0};
    uintptr_t league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00040000u, &score, name, sizeof(name));
    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00400000u, &score, name, sizeof(name));
    }

    if (league_ptr != 0 && score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        uint32_t year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        append_logf("KBO: named league ptr FOUND id=%u ptr=%p score=%d year=%u name=%s",
            league_id, (void*)league_ptr, score, year, name);
        g_kbo_league_ptr_cache_id  = league_id;
        g_kbo_league_ptr_cache_ptr = league_ptr;
        kbo_forget_league_ptr_miss(league_id);
        return league_ptr;
    }

    append_logf("KBO: named league ptr scan missed id=%u best_score=%d name=%s",
        league_id, score, name[0] != '\0' ? name : "(none)");
    kbo_remember_league_ptr_miss(league_id, now_ms);
    return 0;
}
/* ---- native\src\hotkey_window\state_league_name.inc ---- */
static void kbo_hub_read_league_name(uintptr_t league_ptr, char* out, size_t out_size)
{
    if (league_ptr == 0 || out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char name[96] = {0};
    if (!copy_ootp_string_object_text((uint8_t*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, name, sizeof(name))) {
        append_logf("KBO: league name read failed ptr=%p offset=0x%x",
            (void*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET);
        return;
    }

    int score = kbo_hub_league_name_likeness_score(name);
    if (score < 30) {
        append_logf("KBO: league name rejected ptr=%p score=%d name=%s",
            (void*)league_ptr, score, name);
        return;
    }

    append_logf("KBO: league name read ptr=%p offset=0x%x score=%d name=%s",
        (void*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, score, name);
    snprintf(out, out_size, "%s", name);
}

static void kbo_hub_copy_league_display_name_fast(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x97\x86\xec\x9d\x8c", "No league"));
        return;
    }

    if (kbo_hub_try_copy_cached_league_name(league_id, out, out_size)) {
        return;
    }

    snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
}

static void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x97\x86\xec\x9d\x8c", "No league"));
        return;
    }

    if (kbo_hub_try_copy_cached_league_name(league_id, out, out_size)) {
        return;
    }

    uintptr_t league_ptr = kbo_find_league_ptr(league_id);
    if (league_ptr == 0) {
        snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
        return;
    }

    char name[96] = {0};
    kbo_hub_read_league_name(league_ptr, name, sizeof(name));

    uint32_t year = 0;
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }

    if (name[0] != '\0') {
        snprintf(out, out_size, "%s", name);
    } else if (year >= 1982u && year <= 2100u) {
        snprintf(out, out_size, "%s %u / %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id, year);
    } else {
        snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
    }
}
static void kbo_hub_ensure_valid_selection(void);
static void kbo_refresh_hotkey_window(void);
static void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position);
/* ---- native\src\hotkey_window\support\player_badges.inc ---- */
static const char* kbo_hub_foreign_slot_code_for_player(uint8_t* player)
{
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            return "REPL";
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id != 0u) {
            int is_replacement = 0;
            kbo_ensure_foreign_injury_replacements_loaded();
            kbo_lock_foreign_injury_replacements();
            for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
                KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
                if (rec->replacement_player_id == player_id
                        && kbo_foreign_injury_status_uses_slot(rec->status)) {
                    is_replacement = 1;
                    break;
                }
            }
            kbo_unlock_foreign_injury_replacements();
            if (is_replacement) {
                return "REPL";
            }
        }
    }
    return kbo_player_is_asian_quota_candidate(player) ? "AQ" : "REG";
}
/* ---- native\src\hotkey_window\support\skin_assets\style_paths.inc ---- */
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

    append_log_line("KBO F2 hub OOTP install dir unavailable; OOTP skin assets disabled");
}

static void kbo_hub_ootp_install_path(const char* relative_path, char* out, size_t out_size)
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
    kbo_hub_local_asset_path_with_type("icons", file_name, out, out_size);
}

static void kbo_hub_font_asset_path(const char* file_name, char* out, size_t out_size)
{
    kbo_hub_local_asset_path_with_type("fonts", file_name, out, out_size);
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
/* ---- native\src\hotkey_window\support\skin_assets\wic_bitmap.inc ---- */
#include <stdio.h>
#include <string.h>
static void kbo_hub_delete_bitmap(HBITMAP* bitmap)
{
    if (bitmap != NULL && *bitmap != NULL) {
        DeleteObject(*bitmap);
        *bitmap = NULL;
    }
}

static HBITMAP kbo_hub_load_png_hbitmap_wic(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    WCHAR wide_path[MAX_PATH];
    if (!kbo_utf8_to_wide(path, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0])))) {
        return NULL;
    }

    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    HBITMAP bitmap = NULL;

    HRESULT hr = CoCreateInstance(
        &CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,
        (void**)&factory);
    if (FAILED(hr) || factory == NULL) {
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || decoder == NULL) {
        goto cleanup;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || frame == NULL) {
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || converter == NULL) {
        goto cleanup;
    }

    hr = IWICFormatConverter_Initialize(
        converter, (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        goto cleanup;
    }

    UINT width = 0;
    UINT height = 0;
    hr = IWICBitmapSource_GetSize((IWICBitmapSource*)converter, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        goto cleanup;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = (LONG)width;
    bmi.bmiHeader.biHeight      = -(LONG)height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = NULL;
    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (bitmap == NULL || pixels == NULL) {
        bitmap = NULL;
        goto cleanup;
    }

    UINT stride = width * 4u;
    UINT buffer_size = stride * height;
    hr = IWICBitmapSource_CopyPixels((IWICBitmapSource*)converter, NULL, stride, buffer_size, (BYTE*)pixels);
    if (FAILED(hr)) {
        DeleteObject(bitmap);
        bitmap = NULL;
    }

cleanup:
    if (converter != NULL) { IWICFormatConverter_Release(converter); }
    if (frame     != NULL) { IWICBitmapFrameDecode_Release(frame);   }
    if (decoder   != NULL) { IWICBitmapDecoder_Release(decoder);     }
    if (factory   != NULL) { IWICImagingFactory_Release(factory);    }
    return bitmap;
}
/* ---- native\src\hotkey_window\support\skin_assets\asset_lifecycle.inc ---- */
static void kbo_hub_load_skin_assets(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_assets_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH];

    kbo_hub_local_asset_path("github-mark.png", path, sizeof(path));
    g_kbo_hub_asset_github = kbo_hub_load_png_hbitmap_wic(path);
    append_logf("KBO F2 hub github asset path=%s loaded=%d", path, g_kbo_hub_asset_github != NULL);

    kbo_hub_skin_image_path("menu_arrow_right.png", path, sizeof(path));
    g_kbo_hub_asset_menu_arrow = kbo_hub_load_png_hbitmap_wic(path);

    kbo_hub_skin_scrollbar_image_path("sb_bar_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_bottom = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_less_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_less = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_more_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_more = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_bottom = kbo_hub_load_png_hbitmap_wic(path);
}

static void kbo_hub_delete_skin_assets(void)
{
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_github);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_menu_arrow);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_less);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_more);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_league_logo);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_team_logo);
    g_kbo_hub_logo_cache_league_id = 0;
    g_kbo_hub_logo_cache_team_id   = 0;
    g_kbo_hub_logo_cache_year      = 0;
    g_kbo_hub_skin_assets_loaded   = 0;
}
/* ---- native\src\hotkey_window\support\skin_assets\scrollbar_gdi.inc ---- */
static int kbo_hub_estimate_visible_edit_lines(void)
{
    if (g_kbo_hotkey_edit == NULL || g_kbo_hub_content_rect.bottom <= g_kbo_hub_content_rect.top) {
        return 1;
    }

    HDC hdc = GetDC(g_kbo_hotkey_edit);
    if (hdc == NULL) {
        return 1;
    }

    HFONT font = (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES)
        ? g_kbo_hub_font_mono : g_kbo_hub_font_body;
    HGDIOBJ old = NULL;
    if (font != NULL) {
        old = SelectObject(hdc, font);
    }

    TEXTMETRICA metrics;
    memset(&metrics, 0, sizeof(metrics));
    int line_height = 16;
    if (GetTextMetricsA(hdc, &metrics)) {
        line_height = metrics.tmHeight + metrics.tmExternalLeading;
        if (line_height < 8) { line_height = 8; }
    }

    if (old != NULL) { SelectObject(hdc, old); }
    ReleaseDC(g_kbo_hotkey_edit, hdc);

    int content_height = g_kbo_hub_content_rect.bottom - g_kbo_hub_content_rect.top;
    int visible = content_height / line_height;
    return visible > 0 ? visible : 1;
}

static void kbo_hub_update_scrollbar_geometry(void)
{
    SetRectEmpty(&g_kbo_hub_scrollbar_thumb_rect);
    if (g_kbo_hotkey_edit == NULL || g_kbo_hub_scrollbar_rect.bottom <= g_kbo_hub_scrollbar_rect.top) {
        return;
    }

    int button = g_kbo_hub_skin_scrollbar_width;
    if (button < 12) { button = 12; }

    g_kbo_hub_scrollbar_less_rect = g_kbo_hub_scrollbar_rect;
    g_kbo_hub_scrollbar_less_rect.bottom = g_kbo_hub_scrollbar_less_rect.top + button;
    g_kbo_hub_scrollbar_more_rect = g_kbo_hub_scrollbar_rect;
    g_kbo_hub_scrollbar_more_rect.top = g_kbo_hub_scrollbar_more_rect.bottom - button;

    int total_lines   = (int)SendMessageA(g_kbo_hotkey_edit, EM_GETLINECOUNT, 0, 0);
    int first_line    = (int)SendMessageA(g_kbo_hotkey_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    int visible_lines = kbo_hub_estimate_visible_edit_lines();
    int track_top     = g_kbo_hub_scrollbar_less_rect.bottom;
    int track_bottom  = g_kbo_hub_scrollbar_more_rect.top;
    int track_height  = track_bottom - track_top;
    if (track_height <= 0 || total_lines <= visible_lines) {
        g_kbo_hub_scrollbar_thumb_rect.left   = g_kbo_hub_scrollbar_rect.left;
        g_kbo_hub_scrollbar_thumb_rect.right  = g_kbo_hub_scrollbar_rect.right;
        g_kbo_hub_scrollbar_thumb_rect.top    = track_top;
        g_kbo_hub_scrollbar_thumb_rect.bottom = track_bottom;
        return;
    }

    int thumb_h = (visible_lines * track_height) / total_lines;
    if (thumb_h < button)       { thumb_h = button; }
    if (thumb_h > track_height) { thumb_h = track_height; }

    int max_first = total_lines - visible_lines;
    if (max_first < 1) { max_first = 1; }
    if (first_line < 0) { first_line = 0; }
    if (first_line > max_first) { first_line = max_first; }
    int thumb_y = track_top + ((track_height - thumb_h) * first_line) / max_first;

    g_kbo_hub_scrollbar_thumb_rect.left   = g_kbo_hub_scrollbar_rect.left;
    g_kbo_hub_scrollbar_thumb_rect.right  = g_kbo_hub_scrollbar_rect.right;
    g_kbo_hub_scrollbar_thumb_rect.top    = thumb_y;
    g_kbo_hub_scrollbar_thumb_rect.bottom = thumb_y + thumb_h;
}

static void kbo_hub_draw_ootp_scrollbar(HDC hdc)
{
    if (hdc == NULL || g_kbo_hub_scrollbar_rect.right <= g_kbo_hub_scrollbar_rect.left) {
        return;
    }

    kbo_hub_update_scrollbar_geometry();

    RECT track = g_kbo_hub_scrollbar_rect;
    track.top    = g_kbo_hub_scrollbar_less_rect.bottom;
    track.bottom = g_kbo_hub_scrollbar_more_rect.top;
    if (track.bottom > track.top) {
        kbo_hub_draw_vertical_three_piece(
            hdc,
            g_kbo_hub_asset_sb_bar_top,
            g_kbo_hub_asset_sb_bar_mid,
            g_kbo_hub_asset_sb_bar_bottom,
            &track);
    }
    kbo_hub_draw_bitmap_alpha(hdc, g_kbo_hub_asset_sb_less, &g_kbo_hub_scrollbar_less_rect);
    kbo_hub_draw_bitmap_alpha(hdc, g_kbo_hub_asset_sb_more, &g_kbo_hub_scrollbar_more_rect);
    kbo_hub_draw_vertical_three_piece(
        hdc,
        g_kbo_hub_asset_sb_slider_top,
        g_kbo_hub_asset_sb_slider_mid,
        g_kbo_hub_asset_sb_slider_bottom,
        &g_kbo_hub_scrollbar_thumb_rect);
}

static void kbo_hub_init_gdi_objects(void)
{
    kbo_hub_load_skin_metrics();
    kbo_hub_load_skin_assets();

    if (g_kbo_hub_brush_bg         == NULL) { g_kbo_hub_brush_bg         = CreateSolidBrush(KBO_HUB_COLOR_BG);         }
    if (g_kbo_hub_brush_header     == NULL) { g_kbo_hub_brush_header     = CreateSolidBrush(KBO_HUB_COLOR_HEADER);     }
    if (g_kbo_hub_brush_panel      == NULL) { g_kbo_hub_brush_panel      = CreateSolidBrush(KBO_HUB_COLOR_PANEL);      }
    if (g_kbo_hub_brush_panel_alt  == NULL) { g_kbo_hub_brush_panel_alt  = CreateSolidBrush(KBO_HUB_COLOR_PANEL_ALT);  }
    if (g_kbo_hub_brush_nav        == NULL) { g_kbo_hub_brush_nav        = CreateSolidBrush(KBO_HUB_COLOR_NAV);        }
    if (g_kbo_hub_brush_nav_active == NULL) { g_kbo_hub_brush_nav_active = CreateSolidBrush(KBO_HUB_COLOR_NAV_ACTIVE); }
    if (g_kbo_hub_brush_accent     == NULL) { g_kbo_hub_brush_accent     = CreateSolidBrush(KBO_HUB_COLOR_ACCENT);     }

    if (g_kbo_hub_font_title == NULL) {
        g_kbo_hub_font_title = CreateFontW(
            -(g_kbo_hub_skin_article_font_px + 8), 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
    if (g_kbo_hub_font_body == NULL) {
        g_kbo_hub_font_body = CreateFontW(
            -(g_kbo_hub_skin_article_font_px - 1), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
    if (g_kbo_hub_font_mono == NULL) {
        g_kbo_hub_font_mono = CreateFontW(
            -(g_kbo_hub_skin_article_font_px - 2), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
    if (g_kbo_hub_font_small == NULL) {
        g_kbo_hub_font_small = CreateFontW(
            -(g_kbo_hub_skin_button_font_px - 4), 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
}

static void kbo_hub_delete_gdi_objects(void)
{
    if (g_kbo_hub_font_title    != NULL) { DeleteObject(g_kbo_hub_font_title);    g_kbo_hub_font_title    = NULL; }
    if (g_kbo_hub_font_body     != NULL) { DeleteObject(g_kbo_hub_font_body);     g_kbo_hub_font_body     = NULL; }
    if (g_kbo_hub_font_mono     != NULL) { DeleteObject(g_kbo_hub_font_mono);     g_kbo_hub_font_mono     = NULL; }
    if (g_kbo_hub_font_small    != NULL) { DeleteObject(g_kbo_hub_font_small);    g_kbo_hub_font_small    = NULL; }
    if (g_kbo_hub_brush_bg         != NULL) { DeleteObject(g_kbo_hub_brush_bg);         g_kbo_hub_brush_bg         = NULL; }
    if (g_kbo_hub_brush_header     != NULL) { DeleteObject(g_kbo_hub_brush_header);     g_kbo_hub_brush_header     = NULL; }
    if (g_kbo_hub_brush_panel      != NULL) { DeleteObject(g_kbo_hub_brush_panel);      g_kbo_hub_brush_panel      = NULL; }
    if (g_kbo_hub_brush_panel_alt  != NULL) { DeleteObject(g_kbo_hub_brush_panel_alt);  g_kbo_hub_brush_panel_alt  = NULL; }
    if (g_kbo_hub_brush_nav        != NULL) { DeleteObject(g_kbo_hub_brush_nav);        g_kbo_hub_brush_nav        = NULL; }
    if (g_kbo_hub_brush_nav_active != NULL) { DeleteObject(g_kbo_hub_brush_nav_active); g_kbo_hub_brush_nav_active = NULL; }
    if (g_kbo_hub_brush_accent     != NULL) { DeleteObject(g_kbo_hub_brush_accent);     g_kbo_hub_brush_accent     = NULL; }
    kbo_hub_delete_skin_assets();
}

static BOOL CALLBACK kbo_enum_main_window_proc(HWND hwnd, LPARAM lparam)
{
    KboMainWindowSearch* search = (KboMainWindowSearch*)lparam;
    if (search == NULL || hwnd == NULL || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != NULL) {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid) {
        return TRUE;
    }

    if (GetWindowTextLengthA(hwnd) <= 0) {
        return TRUE;
    }

    search->hwnd = hwnd;
    return FALSE;
}

static HWND kbo_find_ootp_main_window(void)
{
    KboMainWindowSearch search;
    memset(&search, 0, sizeof(search));
    search.pid = GetCurrentProcessId();
    EnumWindows(kbo_enum_main_window_proc, (LPARAM)&search);
    return search.hwnd;
}

static int kbo_foreground_is_this_process(void)
{
    HWND foreground = GetForegroundWindow();
    if (foreground == NULL) {
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
}

static void kbo_show_or_hide_hotkey_window(void)
{
    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd == NULL || !IsWindow(hwnd)) {
        return;
    }

    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
        append_logf("KBO F2 hub hidden hwnd=%p", (void*)hwnd);
        return;
    }

    HWND owner = kbo_find_ootp_main_window();
    if (owner != NULL) {
        SetWindowLongPtrA(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
    }

    kbo_hub_prewarm_league_display_cache();
    kbo_refresh_hotkey_window();
    kbo_hub_apply_fixed_window_placement(hwnd, 0);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    append_logf("KBO F2 hub shown hwnd=%p owner=%p", (void*)hwnd, (void*)owner);
}

static int kbo_queue_hotkey_window_toggle(void)
{
    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_hotkey_last_toggle_ms < 250u) {
        return 1;
    }
    g_kbo_hotkey_last_toggle_ms = now;

    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd == NULL || !IsWindow(hwnd)) {
        append_log_line("KBO F2 hub toggle skipped reason=no_window");
        return 0;
    }

    PostMessageA(hwnd, KBO_WM_TOGGLE_SERVICE_MONITOR, 0, 0);
    append_logf("KBO F2 hub toggle queued hwnd=%p", (void*)hwnd);
    return 1;
}

static LRESULT CALLBACK kbo_hotkey_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) && lparam != 0) {
        const KBDLLHOOKSTRUCT* key = (const KBDLLHOOKSTRUCT*)lparam;
        if ((key->vkCode == VK_F2 || key->vkCode == VK_F9) && kbo_foreground_is_this_process()) {
            kbo_queue_hotkey_window_toggle();
            return 1;
        }
        if (key->vkCode == VK_F5
                && g_kbo_hotkey_window != NULL
                && IsWindowVisible(g_kbo_hotkey_window)
                && kbo_foreground_is_this_process()) {
            kbo_refresh_hotkey_window();
            InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
            return 1;
        }
    }

    return CallNextHookEx(g_kbo_hotkey_keyboard_hook, code, wparam, lparam);
}

/* ---- native\src\hotkey_window\support\names.inc ---- */
static void kbo_hub_draw_text(HDC hdc, const char* text, RECT rect, COLORREF color, HFONT font, UINT format)
{
    if (text == NULL) {
        return;
    }

    HFONT old_font = NULL;
    if (font != NULL) {
        old_font = (HFONT)SelectObject(hdc, font);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                DrawTextW(hdc, wide, -1, &rect, format);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        }
    }

    if (old_font != NULL) {
        SelectObject(hdc, old_font);
    }
}

static int kbo_hub_ascii_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int kbo_hub_ascii_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int kbo_hub_ascii_is_alnum(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int kbo_hub_name_fragment_plausible(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 32) {
        return 0;
    }

    int alpha_count = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (kbo_hub_ascii_is_digit(c)) {
            return 0;
        }
        if (c == '_' || c == '/' || c == '\\' || c == ':' || c == '%' || c == '#' || c == '$' || c == '@') {
            return 0;
        }
        if (!(kbo_hub_ascii_is_alpha(c) || c == ' ' || c == '-' || c == '\'' || c == '.')) {
            return 0;
        }
        if (kbo_hub_ascii_is_alpha(c)) {
            alpha_count++;
        }
    }

    if (alpha_count < 2) {
        return 0;
    }
    if (ascii_equals_ignore_case(text, "KBO")
            || ascii_equals_ignore_case(text, "MLB")
            || ascii_equals_ignore_case(text, "SANG")
            || ascii_equals_ignore_case(text, "KPB")
            || ascii_equals_ignore_case(text, "USA")) {
        return 0;
    }

    return 1;
}

static int kbo_hub_copy_raw_pointer_string(uint8_t* object_base, uint32_t pointer_offset, char* out, size_t out_size)
{
    if (object_base == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    uint8_t* pointer_slot = object_base + pointer_offset;
    if (!memory_range_readable(pointer_slot, sizeof(char*))) {
        return 0;
    }
    const char* text = *(const char**)pointer_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static int kbo_hub_append_unique_name_fragment(char fragments[][48], int* count, const char* text)
{
    if (fragments == NULL || count == NULL || *count >= 3 || !kbo_hub_name_fragment_plausible(text)) {
        return 0;
    }
    for (int i = 0; i < *count; i++) {
        if (ascii_equals_ignore_case(fragments[i], text)) {
            return 0;
        }
    }
    snprintf(fragments[*count], 48, "%s", text);
    (*count)++;
    return 1;
}

static void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    kbo_copy_player_display_name(player, out, out_size);
}

static int kbo_hub_team_name_candidate_score(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return -1000;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 64) {
        return -1000;
    }

    int alpha_count = 0;
    int space_count = 0;
    int lowercase_count = 0;
    int uppercase_count = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c < 0x20 || c > 0x7e || kbo_hub_ascii_is_digit(c)) {
            return -1000;
        }
        if (kbo_hub_ascii_is_alpha(c)) {
            alpha_count++;
            if (c >= 'a' && c <= 'z') { lowercase_count++; } else { uppercase_count++; }
        } else if (c == ' ') {
            space_count++;
        } else if (!(c == '-' || c == '\'' || c == '.' || c == '&')) {
            return -1000;
        }
    }

    if (alpha_count < 2) {
        return -1000;
    }

    int score = (int)len;
    if (space_count > 0) { score += 80; }
    if (lowercase_count > 0) { score += 20; }
    if (space_count == 0 && len <= 5 && uppercase_count == alpha_count) { score -= 80; }
    if (ascii_equals_ignore_case(text, "SANG") || ascii_equals_ignore_case(text, "KPB")) { score -= 200; }

    return score;
}

static void kbo_hub_copy_team_display_name_from_ptr(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
        return;
    }

    static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };

    char city[64] = {0};
    char nickname[64] = {0};
    char best[96] = {0};
    int best_score = -1000;

    copy_ootp_string_object_text(team, 0x10u, city,     sizeof(city));
    copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));

    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (!copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))) {
            continue;
        }
        int score = kbo_hub_team_name_candidate_score(text);
        if (score > best_score) {
            best_score = score;
            snprintf(best, sizeof(best), "%s", text);
        }
    }

    if (best_score > 0 && strchr(best, ' ') != NULL) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    if (kbo_hub_team_name_candidate_score(city) > 0
            && kbo_hub_team_name_candidate_score(nickname) > 0
            && !ascii_equals_ignore_case(city, nickname)) {
        snprintf(out, out_size, "%s %s", city, nickname);
        return;
    }

    if (best_score > 0) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
}

static void kbo_hub_copy_team_display_name_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL) {
        kbo_hub_copy_team_display_name_from_ptr(team, out, out_size, fallback);
        return;
    }

    if (fallback != NULL && fallback[0] != '\0') {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0) {
        snprintf(out, out_size, "Unknown club #%u", team_id);
    } else {
        snprintf(out, out_size, "Unknown club");
    }
}

static void kbo_hub_copy_team_abbrev_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint32_t parent_team_id = 0u;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (parent_team_id != 0u && parent_team_id != team_id) {
        char parent_abbrev[16] = {0};
        kbo_hub_copy_team_abbrev_by_id(parent_team_id, parent_abbrev, sizeof(parent_abbrev), fallback);
        if (parent_abbrev[0] != '\0' && strcmp(parent_abbrev, "-") != 0) {
            snprintf(out, out_size, "%s2", parent_abbrev);
            return;
        }
    }

    char name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, name, sizeof(name), fallback);
    if (kbo_ascii_contains_ignore_case(name, "Lotte"))      { snprintf(out, out_size, "LOT");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Doosan"))     { snprintf(out, out_size, "DOO");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Samsung"))    { snprintf(out, out_size, "SAM");  return; }
    if (kbo_ascii_contains_ignore_case(name, "KIA"))        { snprintf(out, out_size, "KIA");  return; }
    if (kbo_ascii_contains_ignore_case(name, "SSG"))        { snprintf(out, out_size, "SSG");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Hanwha"))     { snprintf(out, out_size, "HAN");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Kiwoom"))     { snprintf(out, out_size, "KIW");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Nexen"))      { snprintf(out, out_size, "NEX");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Heroes"))     { snprintf(out, out_size, "KIW");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Dinos"))      { snprintf(out, out_size, "NC");   return; }
    if (kbo_ascii_contains_ignore_case(name, "NC"))         { snprintf(out, out_size, "NC");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Wiz"))        { snprintf(out, out_size, "KT");   return; }
    if (kbo_ascii_contains_ignore_case(name, "KT"))         { snprintf(out, out_size, "KT");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Twins"))      { snprintf(out, out_size, "LG");   return; }
    if (kbo_ascii_contains_ignore_case(name, "LG"))         { snprintf(out, out_size, "LG");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Sangmu"))     { snprintf(out, out_size, "SANG"); return; }
    if (kbo_ascii_contains_ignore_case(name, "Police"))     { snprintf(out, out_size, "KPB");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Ulsan"))      { snprintf(out, out_size, "ULS");  return; }

    if (fallback != NULL && fallback[0] != '\0' && strlen(fallback) <= 6u) {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0u) {
        snprintf(out, out_size, "T%u", team_id);
    } else {
        snprintf(out, out_size, "-");
    }
}

/* ---- native\src\hotkey_window\support\logos.inc ---- */
static void kbo_hub_sanitize_logo_prefix(const char* name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (name == NULL) {
        return;
    }

    size_t write = 0;
    int last_was_sep = 1;
    for (size_t read = 0; name[read] != '\0' && write + 1 < out_size; read++) {
        char ch = name[read];
        if (kbo_hub_ascii_is_alnum(ch)) {
            out[write++] = ch;
            last_was_sep = 0;
        } else if (!last_was_sep) {
            out[write++] = '_';
            last_was_sep = 1;
        }
    }
    while (write > 0 && out[write - 1] == '_') {
        write--;
    }
    out[write] = '\0';
}

static int kbo_hub_parse_four_digits(const char* text, int index)
{
    int value = 0;
    for (int i = 0; i < 4; i++) {
        char ch = text[index + i];
        if (ch < '0' || ch > '9') {
            return -1;
        }
        value = (value * 10) + (ch - '0');
    }
    return value;
}

static int kbo_hub_logo_filename_score(const char* file_name, uint32_t year)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return -1000000;
    }

    int has_year_token = 0;
    int best_year_score = year == 0 ? 0 : -100000;
    size_t len = strlen(file_name);
    for (size_t i = 0; i + 4 <= len; i++) {
        int start_year = kbo_hub_parse_four_digits(file_name, (int)i);
        if (start_year < 1800 || start_year > 3000) {
            continue;
        }
        has_year_token = 1;
        int score = -5000;
        if (i + 9 <= len && file_name[i + 4] == '-') {
            int end_year = kbo_hub_parse_four_digits(file_name, (int)i + 5);
            if (end_year >= start_year) {
                score = (year >= (uint32_t)start_year && year <= (uint32_t)end_year)
                    ? 10000 - abs((int)year - start_year) : -1000;
                i += 8;
            }
        } else if (year == (uint32_t)start_year) {
            score = 12000;
        }
        if (score > best_year_score) {
            best_year_score = score;
        }
    }

    int score = has_year_token ? best_year_score : 100;
    if (strstr(file_name, "_small_50") != NULL) { score += 700; }
    if (strstr(file_name, "_away")     != NULL
            || strstr(file_name, "_Away") != NULL) { score -= 500; }
    return score;
}

static int kbo_hub_find_best_logo_file(
    const char* directory, const char* search_prefix,
    uint32_t year, char* out, size_t out_size)
{
    if (directory == NULL || search_prefix == NULL || search_prefix[0] == '\0'
            || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\%s*.*", directory, search_prefix);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int best_score = -1000000;
    char best_path[MAX_PATH] = {0};
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        size_t name_len = strlen(find_data.cFileName);
        int is_png = name_len >= 4 && ascii_equals_ignore_case(find_data.cFileName + name_len - 4, ".png");
        int is_oi  = name_len >= 3 && ascii_equals_ignore_case(find_data.cFileName + name_len - 3, ".oi");
        if (!is_png && !is_oi) {
            continue;
        }
        int score = kbo_hub_logo_filename_score(find_data.cFileName, year);
        if (is_png) { score += 50; }
        if (score > best_score) {
            best_score = score;
            snprintf(best_path, sizeof(best_path), "%s\\%s", directory, find_data.cFileName);
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    if (best_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", best_path);
    return 1;
}

static const char* kbo_hub_file_name_part(const char* path)
{
    if (path == NULL) {
        return NULL;
    }

    const char* name = path;
    for (const char* p = path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            name = p + 1;
        }
    }
    return name;
}

static void kbo_hub_copy_logo_prefix_from_file_name(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (file_name == NULL || file_name[0] == '\0') {
        return;
    }

    snprintf(out, out_size, "%s", file_name);
    char* dot = strrchr(out, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
}

static int kbo_hub_copy_existing_file(const char* path, char* out, size_t out_size)
{
    if (path == NULL || path[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return 0;
    }

    snprintf(out, out_size, "%s", path);
    return 1;
}

static int kbo_hub_find_cached_league_logo_in_dir(
    const char* directory,
    const char* logo_file,
    uint32_t year,
    char* out,
    size_t out_size)
{
    if (directory == NULL || directory[0] == '\0'
            || logo_file == NULL || logo_file[0] == '\0'
            || out == NULL || out_size == 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\%s", directory, logo_file);
    if (kbo_hub_copy_existing_file(path, out, out_size)) {
        return 1;
    }

    char prefix[128] = {0};
    kbo_hub_copy_logo_prefix_from_file_name(logo_file, prefix, sizeof(prefix));
    if (prefix[0] == '\0') {
        return 0;
    }

    snprintf(path, sizeof(path), "%s\\%s.oi", directory, prefix);
    if (kbo_hub_copy_existing_file(path, out, out_size)) {
        return 1;
    }

    return kbo_hub_find_best_logo_file(directory, prefix, year, out, out_size);
}

static int kbo_hub_get_league_logo_path(uint32_t league_id, uint32_t year, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char cached_logo_file_raw[128] = {0};
    if (!kbo_hub_try_copy_cached_league_logo_file(league_id, cached_logo_file_raw, sizeof(cached_logo_file_raw))) {
        return 0;
    }

    const char* cached_logo_file = kbo_hub_file_name_part(cached_logo_file_raw);
    if (cached_logo_file == NULL || cached_logo_file[0] == '\0') {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (kbo_get_current_save_path(save_path, sizeof(save_path))) {
        char save_logo_dir[MAX_PATH] = {0};
        snprintf(save_logo_dir, sizeof(save_logo_dir), "%s\\news\\html\\images\\league_logos", save_path);
        if (kbo_hub_find_cached_league_logo_in_dir(save_logo_dir, cached_logo_file, year, out, out_size)) {
            return 1;
        }
    }

    char data_logo_dir[MAX_PATH] = {0};
    kbo_hub_ootp_install_path("data\\logos", data_logo_dir, sizeof(data_logo_dir));
    if (kbo_hub_find_cached_league_logo_in_dir(data_logo_dir, cached_logo_file, year, out, out_size)) {
        return 1;
    }

    return 0;
}

static int kbo_hub_get_team_logo_path(uint32_t team_id, uint32_t year, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    char prefix[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, team_name, sizeof(team_name), NULL);
    kbo_hub_sanitize_logo_prefix(team_name, prefix, sizeof(prefix));
    if (prefix[0] == '\0') {
        return 0;
    }

    char data_logo_dir[MAX_PATH] = {0};
    kbo_hub_ootp_install_path("data\\logos", data_logo_dir, sizeof(data_logo_dir));
    if (kbo_hub_find_best_logo_file(data_logo_dir, prefix, year, out, out_size)) {
        return 1;
    }

    char cap_prefix[112] = {0};
    snprintf(cap_prefix, sizeof(cap_prefix), "caps_%s", prefix);
    char ballcap_dir[MAX_PATH] = {0};
    kbo_hub_ootp_install_path("data\\ballcaps", ballcap_dir, sizeof(ballcap_dir));
    return kbo_hub_find_best_logo_file(ballcap_dir, cap_prefix, year, out, out_size);
}

static void kbo_hub_ensure_header_logos(uint32_t league_id, uint32_t team_id, uint32_t year)
{
    if (g_kbo_hub_logo_cache_league_id == league_id
            && g_kbo_hub_logo_cache_team_id == team_id
            && g_kbo_hub_logo_cache_year == year) {
        return;
    }

    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_league_logo);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_team_logo);
    g_kbo_hub_logo_cache_league_id = league_id;
    g_kbo_hub_logo_cache_team_id   = team_id;
    g_kbo_hub_logo_cache_year      = year;

    char path[MAX_PATH] = {0};
    kbo_hub_get_league_logo_path(league_id, year, path, sizeof(path));
    g_kbo_hub_asset_selected_league_logo = kbo_hub_load_png_hbitmap_wic(path);

    if (kbo_hub_get_team_logo_path(team_id, year, path, sizeof(path))) {
        g_kbo_hub_asset_selected_team_logo = kbo_hub_load_png_hbitmap_wic(path);
    }
}
/* ---- native\src\hotkey_window\support\team_colors.inc ---- */
#include <stdio.h>
#include <string.h>
static int kbo_hub_argb_team_color_to_hex(uint32_t argb, char* out, size_t out_size)
{
    if (out == NULL || out_size < 8) {
        return 0;
    }

    uint32_t alpha = (argb >> 24u) & 0xffu;
    uint32_t rgb = argb & 0x00ffffffu;
    if (alpha == 0u && rgb == 0u) {
        return 0;
    }

    snprintf(out, out_size, "#%06X", rgb);
    return 1;
}

static int kbo_hub_copy_team_bar_colors(uint32_t team_id, char* primary, size_t primary_size, char* secondary, size_t secondary_size)
{
    if (primary != NULL && primary_size > 0) {
        snprintf(primary, primary_size, "#f04a22");
    }
    if (secondary != NULL && secondary_size > 0) {
        snprintf(secondary, secondary_size, "#2c2c2c");
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_PRIMARY_COLOR_OFFSET, 12u)) {
        return 0;
    }

    uint32_t primary_argb = *(uint32_t*)(team + OOTP27_KBO_TEAM_PRIMARY_COLOR_OFFSET);
    uint32_t secondary_argb = *(uint32_t*)(team + OOTP27_KBO_TEAM_SECONDARY_COLOR_OFFSET);
    int primary_ok = kbo_hub_argb_team_color_to_hex(primary_argb, primary, primary_size);
    int secondary_ok = kbo_hub_argb_team_color_to_hex(secondary_argb, secondary, secondary_size);
    return primary_ok && secondary_ok;
}
/* ---- native\src\hotkey_window\content_service_helpers.inc ---- */
static int kbo_hub_count_service_players(uint32_t service_team_id, int* out_due_60, int* out_due_now)
{
    if (out_due_60 != NULL) { *out_due_60 = 0; }
    if (out_due_now != NULL) { *out_due_now = 0; }
    if (service_team_id == 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    int due_60 = 0;
    int due_now = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            continue;
        }
        int32_t days_left = kbo_military_effective_days_left(player);
        if (days_left <= 0) { due_now++; }
        if (days_left > 0 && days_left <= 60) { due_60++; }
        count++;
    }

    if (out_due_60 != NULL)  { *out_due_60  = due_60;  }
    if (out_due_now != NULL) { *out_due_now = due_now; }
    return count;
}
/* ---- native\src\hotkey_window\content_overview.inc ---- */
static void kbo_build_overview_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    int sang_count = kbo_hub_count_service_players(sang_id, NULL, NULL);
    int kpb_count  = kbo_hub_count_service_players(kpb_id,  NULL, NULL);

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xec\x9a\x94\xec\x95\xbd", "OVERVIEW"));

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80", "SERVICE TEAMS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xec\x83\x81\xeb\xac\xb4 \xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91: %d\xeb\xaa\x85\r\n", "  Sangmu serving: %d\r\n"),
        sang_count);
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xea\xb2\xbd\xec\xb0\xb0 \xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91: %d\xeb\xaa\x85\r\n", "  Police serving: %d\r\n"),
        kpb_count);
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xec\xb6\x94\xec\xa0\x81 \xec\xa4\x91\xec\x9d\xb8 \xeb\xb3\xb5\xeb\xac\xb4 \xeb\xb0\xb0\xec\xa0\x95: %ld\xea\xb1\xb4\r\n\r\n", "  Tracked assignments: %ld\r\n\r\n"),
        g_active_military_loan_count);

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c", "PATCH STATUS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  KBO Fix: %s\r\n", "  KBO Fix: %s\r\n"),
        kbo_fix_enabled() ? kbo_hub_text("\xec\xbc\x9c\xec\xa7\x90", "enabled") : kbo_hub_text("\xea\xba\xbc\xec\xa7\x90", "disabled"));
    char foreign_waiver_status[220] = {0};
    if (kbo_get_foreign_waiver_window_status_text(foreign_waiver_status, sizeof(foreign_waiver_status))) {
        kbo_window_text_appendf(&buffer, "%s\r\n", foreign_waiver_status);
    }
}
/* ---- native\src\hotkey_window\content_military.inc ---- */
static void kbo_build_military_service_window_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "Sangmu Baseball Team");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "Korean Police Baseball Team");

    int sang_due_60 = 0;
    int sang_due_now = 0;
    int kpb_due_60 = 0;
    int kpb_due_now = 0;
    int sang_count = kbo_hub_count_service_players(sang_id, &sang_due_60, &sang_due_now);
    int kpb_count  = kbo_hub_count_service_players(kpb_id,  &kpb_due_60,  &kpb_due_now);

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80", "SERVICE TEAMS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("\xec\xb6\x94\xec\xa0\x81 \xec\xa4\x91\xec\x9d\xb8 \xeb\xb3\xb5\xeb\xac\xb4 \xeb\xb0\xb0\xec\xa0\x95: %ld\r\n\r\n", "Tracked service assignments: %ld\r\n\r\n"),
        g_active_military_loan_count);

    kbo_window_text_appendf(
        &buffer, "%s: serving=%d, returning soon=%d, ready now=%d\r\n",
        sang_name, sang_count, sang_due_60, sang_due_now);
    kbo_window_text_appendf(
        &buffer, "%s: serving=%d, returning soon=%d, ready now=%d\r\n\r\n",
        kpb_name, kpb_count, kpb_due_60, kpb_due_now);

    uintptr_t player_vector = 0;
    int32_t   player_count  = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "CURRENT SERVICE LIST\r\n");
    kbo_window_text_appendf(&buffer, "PLAYER                   SERVICE TEAM              ORIGINAL CLUB             RETURN DATE  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "------------------------------------------------------------------------------------------------\r\n");

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        const char* service_fallback = NULL;
        uint32_t service_team_id = 0;
        if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
            service_team_id  = sang_id;
            service_fallback = sang_name;
        } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
            service_team_id  = kpb_id;
            service_fallback = kpb_name;
        }

        if (service_team_id == 0) {
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id);
        kbo_military_repair_original_team_memory(
            player,
            original_team_id,
            original_league_id,
            service_team_id,
            sang_id,
            kpb_id);
        int32_t days_left     = kbo_military_effective_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        uint8_t loan_active     = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        const char* status = days_left <= 0 ? "Ready to return" : (days_left <= 60 ? "Returning soon" : "Serving");
        if      (military_active == 0) { status = "Needs review"; }
        else if (loan_active     == 0) { status = "Club-only";    }

        char player_name[64]       = {0};
        char service_team_name[64] = {0};
        char original_team_name[64] = {0};
        char return_date[16] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        kbo_hub_copy_team_display_name_by_id(service_team_id,  service_team_name,  sizeof(service_team_name),  service_fallback);
        kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);
        kbo_military_format_yyyymmdd(
            kbo_military_effective_return_yyyymmdd(player),
            return_date,
            sizeof(return_date));

        kbo_window_text_appendf(
            &buffer,
            "%-24.24s %-25.25s %-25.25s %-11.11s  %s\r\n",
            player_name, service_team_name, original_team_name,
            return_date, status);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}
/* ---- native\src\hotkey_window\content_foreign_rights.inc ---- */
static void kbo_build_foreign_rights_window_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint32_t selected_team_id = g_kbo_hub_selected_team_id;
    char selected_team_name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(selected_team_id, selected_team_name, sizeof(selected_team_name), NULL);

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "FOREIGN PLAYER RIGHTS"));
    kbo_window_text_appendf(&buffer, "%s: %s (%u)\r\n", kbo_hub_text("\xed\x8c\x80", "Team"), selected_team_name, selected_team_id);
    char window_status[256] = {0};
    kbo_get_foreign_waiver_window_status_text(window_status, sizeof(window_status));
    kbo_window_text_appendf(&buffer, "%s\r\n", window_status);
    kbo_window_text_appendf(&buffer, "\r\n");
    kbo_window_text_appendf(&buffer, "HOW TO USE\r\n");
    kbo_window_text_appendf(&buffer, "  1. Choose a team from the top-right team dropdown.\r\n");
    kbo_window_text_appendf(&buffer, "  2. Select a foreign player in the list.\r\n");
    kbo_window_text_appendf(&buffer, "  3. Click KEEP RIGHTS or RELEASE RIGHTS.\r\n\r\n");

    uint32_t top_player_id = 0;
    uint32_t top_current_team_id = 0;
    if (kbo_resolve_foreign_waiver_top_candidate_for_team(selected_team_id, &top_player_id, &top_current_team_id)) {
        char top_player_name[64] = {0};
        uint8_t* top_player = kbo_find_player_by_id(top_player_id, NULL, NULL);
        if (top_player != NULL) {
            kbo_hub_copy_player_display_name(top_player, top_player_name, sizeof(top_player_name));
        }
        if (g_kbo_hub_selected_foreign_player_id == 0u) {
            g_kbo_hub_selected_foreign_player_id = top_player_id;
        }
        if (g_kbo_hub_selected_foreign_player_id == top_player_id) {
            kbo_window_text_appendf(&buffer, "SELECTED: %s (%u)\r\n\r\n", top_player_name, top_player_id);
        } else {
            char selected_name[64] = {0};
            uint8_t* selected_player = kbo_find_player_by_id(g_kbo_hub_selected_foreign_player_id, NULL, NULL);
            if (selected_player != NULL) {
                kbo_hub_copy_player_display_name(selected_player, selected_name, sizeof(selected_name));
                kbo_window_text_appendf(&buffer, "SELECTED: %s (%u)\r\n\r\n", selected_name, g_kbo_hub_selected_foreign_player_id);
            } else {
                kbo_window_text_appendf(&buffer, "SELECTED: %s (%u)\r\n\r\n", top_player_name, top_player_id);
            }
        }
    } else {
        kbo_window_text_appendf(&buffer, "SELECTED: (none)\r\n\r\n");
    }

    if (selected_team_id == 0) {
        kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xed\x8c\x80\xec\x9d\x84 \xeb\xa8\xbc\xec\xa0\x80 \xec\x84\xa0\xed\x83\x9d\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.", "Select a team first."));
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "FOREIGN PLAYERS ON THIS TEAM\r\n");
    kbo_window_text_appendf(&buffer, "  PLAYER                   ID          TEAM       NAT     STATUS\r\n");
    kbo_window_text_appendf(&buffer, "--------------------------------------------------------------------------\r\n");

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int forced = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        (void)forced;
        (void)score;
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        if (player_id == 0 || decision_team_id != selected_team_id
                || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        char player_name[64] = {0};
        char flags[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        snprintf(flags, sizeof(flags), "%s%s%s%s%s",
            restricted ? "Restricted " : "",
            secondary ? "SecRestricted " : "",
            dfa ? "DFA " : "",
            loan_active ? "Loan " : "",
            inj_active ? "Injured " : "");
        if (flags[0] == '\0') {
            snprintf(flags, sizeof(flags), "Active");
        }

        kbo_window_text_appendf(
            &buffer,
            "%c %-24.24s %-11u %-10u %-7u %s\r\n",
            player_id == g_kbo_hub_selected_foreign_player_id ? '>' : ' ',
            player_name,
            player_id,
            current_team_id,
            nation_id,
            flags);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}
/* ---- native\src\hotkey_window\content_foreign_injury.inc ---- */
static void kbo_build_foreign_injury_replacement_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint32_t selected_team_id = g_kbo_hub_selected_team_id;
    char selected_team_name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(selected_team_id, selected_team_name, sizeof(selected_team_name), NULL);
    kbo_foreign_injury_replacement_scan_once("hotkey_text");

    int open_count = 0;
    int pending_count = 0;
    int closed_count = 0;
    kbo_count_foreign_injury_replacements_for_team(selected_team_id, &open_count, &pending_count, &closed_count);

    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);

    kbo_window_text_appendf(&buffer, "FOREIGN INJURY REPLACEMENT\r\n");
    kbo_window_text_appendf(&buffer, "Team: %s (%u)\r\n", selected_team_name, selected_team_id);
    kbo_window_text_appendf(&buffer, "Open: %d / Decision due: %d / Closed: %d\r\n", open_count, pending_count, closed_count);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  SLOT          INJURED PLAYER            ID          REPLACEMENT              ID          DAYS  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "--------------------------------------------------------------------------------------------------------\r\n");

    int rendered = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count && rendered < 500; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (selected_team_id != 0u && rec->team_id != selected_team_id) {
            continue;
        }

        char player_name[64] = {0};
        char replacement_name[64] = {0};
        int days_left = 0;
        uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            days_left = (int)*(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        } else {
            snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
        }
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        if (replacement != NULL && memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_hub_copy_player_display_name(replacement, replacement_name, sizeof(replacement_name));
        } else if (rec->replacement_player_id != 0u) {
            snprintf(replacement_name, sizeof(replacement_name), "Player #%u", rec->replacement_player_id);
        } else {
            snprintf(replacement_name, sizeof(replacement_name), "-");
        }

        kbo_window_text_appendf(
            &buffer,
            "  %-13.13s %-24.24s %-11u %-24.24s %-11u %-5d %s\r\n",
            kbo_foreign_injury_slot_label(rec->slot_type),
            player_name,
            rec->injured_player_id,
            replacement_name,
            rec->replacement_player_id,
            days_left,
            kbo_foreign_injury_status_label(rec->status));
        rendered++;
    }
    kbo_unlock_foreign_injury_replacements();

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}
/* ---- native\src\hotkey_window\content_mod_info.inc ---- */
static void kbo_build_mod_info_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO"));
    kbo_window_text_appendf(&buffer, "GitHub\r\n");
    kbo_window_text_appendf(&buffer, "  https://github.com/lebronisbest623/OOTP27_Ultimate_KBO\r\n\r\n");
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "Ultimate KBO \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 \xec\x98\xa4\xec\xa7\x81 OOTP\xec\x97\x90\xec\x84\x9c \xec\xb5\x9c\xea\xb3\xa0\xec\x9d\x98 KBO \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xec\x9c\x84\xed\x95\xb4 \xeb\xa7\x8c\xeb\x93\xa4\xec\x96\xb4\xec\xa7\x84 \xeb\xaa\xa8\xeb\x93\x9c\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
            "Ultimate KBO is built only to provide the best KBO experience in OOTP."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "\xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 KBO \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98\xed\x95\xa0 \xeb\x95\x8c \xec\xb5\x9c\xec\x83\x81\xec\x9d\x98 \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
            "The mod is best experienced together with the KBO Launcher."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "\xeb\xb3\xb8 \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 OOTPD\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
            "This mod is not an official OOTPD product."));
    for (size_t i = 0; i < kbo_supported_ootp_build_count(); i++) {
        const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
        if (build == NULL) {
            continue;
        }
        kbo_window_text_appendf(
            &buffer,
            "Supported build: OOTP 27 %s / timestamp 0x%08X / image 0x%08X\r\n",
            build->label,
            build->timestamp,
            build->size_of_image);
    }
}
/* ---- native\src\hotkey_window\content_foreign_policy.inc ---- */
static void kbo_build_foreign_policy_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    int configured_nations = kbo_load_asian_quota_nation_ids_once();
    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);
    kbo_window_text_appendf(&buffer, "KBO FOREIGN PLAYERS\r\n");
    kbo_window_text_appendf(
        &buffer,
        "Policy: %s / Base effective limit: %u / Asian-quota nation IDs: %d\r\n\r\n",
        kbo_custom_foreign_policy_enabled() ? "Custom KBO layer" : "OOTP fallback",
        KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        configured_nations);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  SLOT  PLAYER                   ID          TEAM       ACTIVE     NAT     STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------\r\n");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
        return;
    }

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (player_id == 0u
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
            continue;
        }

        char player_name[64] = {0};
        char flags[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        snprintf(flags, sizeof(flags), "%s%s%s%s%s",
            player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
            player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
            player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
            player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
            player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
        if (flags[0] == '\0') {
            snprintf(flags, sizeof(flags), "Active");
        }

        kbo_window_text_appendf(
            &buffer,
            "  %-5.5s %-24.24s %-11u %-10u %-10u %-7u %s\r\n",
            kbo_hub_foreign_slot_code_for_player(player),
            player_name,
            player_id,
            current_team_id,
            active_team_id,
            nation_id,
            flags);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }

}
/* ---- native\src\hotkey_window\content_asian_games.inc ---- */
static void kbo_build_asian_games_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_clear_asian_games_roster_if_save_changed("hotkey_text");

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0) {
        kbo_load_asian_games_roster_csv("hotkey_text");
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int departed = 0;
    int returned = 0;
    int exempted = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].departed) { departed++; }
        if (g_kbo_asian_games_roster[i].returned) { returned++; }
        if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
    }

    kbo_window_text_appendf(&buffer, "ASIAN GAMES ROSTER\r\n");
    kbo_window_text_appendf(
        &buffer,
        "Year: %u / Selected: %ld / Departed: %d / Returned: %d / Exempted: %d\r\n\r\n",
        g_kbo_asian_games_roster_year,
        roster_count,
        departed,
        returned,
        exempted);

    if (roster_count == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
        kbo_window_text_appendf(&buffer, "Advance through the roster-selection event, then refresh this panel.\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "  #  PLAYER                   ID          TEAM       LG         AGE ROLE WC  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------------\r\n");

    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        uintptr_t player_ptr = entry->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
        }

        char player_name[64] = {0};
        if (kbo_player_pointer_plausible(player_ptr)) {
            kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
        } else {
            snprintf(player_name, sizeof(player_name), "Unknown player");
        }

        const char* status = "Selected";
        if (entry->returned) {
            status = entry->exempted ? "Returned/Exempt" : "Returned";
        } else if (entry->departed) {
            status = "Departed/IL";
        }

        kbo_window_text_appendf(
            &buffer,
            "  %02ld %-24.24s %-11u %-10u %-10u %-3u %-4s %-3s %s\r\n",
            i + 1,
            player_name,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->age,
            kbo_asian_games_role_bucket_label(entry->role),
            entry->wildcard ? "YES" : "NO",
            status);
    }
}
/* ---- native\src\hotkey_window\content_fa_cases.inc ---- */
static void kbo_build_fa_cases_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    if (rows == NULL) {
        kbo_window_text_appendf(&buffer, "FA MARKET\r\n\r\nCould not allocate classification buffer.\r\n");
        return;
    }

    KboFaMarketScanSummary summary = {0};
    int count = kbo_collect_fa_market_classifications(
        g_kbo_hub_selected_league_id,
        rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &summary,
        1,
        "f2_text");

    char league_name[128] = {0};
    kbo_hub_copy_league_display_name(summary.league_id, league_name, sizeof(league_name));

    kbo_window_text_appendf(&buffer, "FA MARKET\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n", league_name[0] != '\0' ? league_name : "Selected league");
    kbo_window_text_appendf(
        &buffer,
        "Scanned %d players / %d active teamless candidates / %d rows. Seed rows: %d. CSV: %s\r\n\r\n",
        summary.scanned,
        summary.candidates,
        count,
        summary.seed_count,
        summary.csv_path[0] != '\0' ? summary.csv_path : "-");
    kbo_window_text_appendf(&buffer, "PLAYER                   CASE            GRADE    PREV SALARY TEAM AGE RIGHTS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < count && i < 600; i++) {
        KboFaMarketClassification* row = &rows[i];
        char team_abbrev[16] = "-";
        char rights_abbrev[16] = "-";
        char salary_text[32] = "-";
        kbo_fa_market_format_salary(row->fa_grade_salary, salary_text, sizeof(salary_text));
        kbo_hub_copy_team_abbrev_by_id(
            kbo_fa_market_display_team_id(row),
            team_abbrev,
            sizeof(team_abbrev),
            "-");
        kbo_hub_copy_team_abbrev_by_id(row->rights_team_id, rights_abbrev, sizeof(rights_abbrev), "-");
        kbo_window_text_appendf(
            &buffer,
            "%-24.24s %-15.15s %-5.5s %14s %-4.4s %3u %-6.6s\r\n",
            row->player_name,
            kbo_fa_market_display_case_label(row->case_label),
            kbo_fa_market_display_grade(row->grade),
            salary_text,
            team_abbrev,
            (uint32_t)row->age,
            rights_abbrev);
    }
    if (count == 0) {
        kbo_window_text_appendf(&buffer, "No active players without a current team found.\r\n");
    } else if (summary.truncated || count > 600) {
        kbo_window_text_appendf(&buffer, "\r\nOutput truncated. Open the CSV for the full snapshot.\r\n");
    }

    HeapFree(GetProcessHeap(), 0, rows);
}
/* ---- native\src\hotkey_window\content_fa_compensation.inc ---- */
static void kbo_build_fa_compensation_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        kbo_window_text_appendf(&buffer, "FA COMPENSATION\r\n\r\nCould not allocate compensation buffer.\r\n");
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));

    kbo_window_text_appendf(&buffer, "FA COMPENSATION\r\n");
    kbo_window_text_appendf(&buffer, "Rows: %d. CSV: %s\r\n\r\n", count, path[0] != '\0' ? path : "-");
    kbo_window_text_appendf(&buffer, "DATE     PLAYER                   GRADE FROM TO   PREV SALARY  CASH+PLAYER  CASH ONLY   PROT STATUS\r\n");
    kbo_window_text_appendf(&buffer, "------------------------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < count && i < 600; i++) {
        KboFaCompensationRecord* rec = &records[i];
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";
        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));
        kbo_window_text_appendf(
            &buffer,
            "%8u %-24.24s %-5.5s %-4.4s %-4.4s %12s %12s %12s %4u %-8.8s\r\n",
            rec->signed_on_yyyymmdd,
            rec->player_name,
            kbo_fa_market_display_grade(rec->grade),
            original_team,
            signing_team,
            previous_salary,
            cash_with_player,
            cash_only,
            rec->protect_count,
            kbo_fa_compensation_status_label(rec->status));
    }
    if (count == 0) {
        kbo_window_text_appendf(&buffer, "No KBO FA compensation obligations recorded.\r\n");
    } else if (count > 600) {
        kbo_window_text_appendf(&buffer, "\r\nOutput truncated. Open the CSV for the full ledger.\r\n");
    }

    HeapFree(GetProcessHeap(), 0, records);
}
/* ---- native\src\hotkey_window\content_settings.inc ---- */
static void kbo_build_settings_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "SETTINGS"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "LEAGUE SETTINGS"));
    int multiplier = kbo_get_intl_established_fa_multiplier();
    kbo_window_text_appendf(&buffer, "%s\r\n", "INTERNATIONAL ESTABLISHED FA");
    kbo_window_text_appendf(
        &buffer,
        "  Multiplier: %dx (OOTP A Lot 12 -> about %d)\r\n",
        multiplier,
        12 * multiplier);
}
/* ---- native\src\hotkey_window\content_router.inc ---- */
static void kbo_build_hub_window_text(char* out, size_t out_size)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: kbo_build_military_service_window_text(out, out_size); return;
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: kbo_build_foreign_rights_window_text(out, out_size); return;
    case KBO_HUB_VIEW_ASIAN_QUOTA:
        if (g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS) {
            kbo_build_foreign_rights_window_text(out, out_size);
        } else {
            kbo_build_foreign_policy_hub_text(out, out_size);
        }
        return;
    case KBO_HUB_VIEW_ASIAN_GAMES: kbo_build_asian_games_hub_text(out, out_size);       return;
    case KBO_HUB_VIEW_UPCOMING_FA:
    case KBO_HUB_VIEW_FA_CASES:
        if (g_kbo_hub_selected_fa_subview == KBO_HUB_FA_SUBVIEW_COMPENSATION) {
            kbo_build_fa_compensation_hub_text(out, out_size);
        } else {
            kbo_build_fa_cases_hub_text(out, out_size);
        }
        return;
    case KBO_HUB_VIEW_MOD_INFO: kbo_build_mod_info_hub_text(out, out_size);            return;
    case KBO_HUB_VIEW_SETTINGS: kbo_build_settings_hub_text(out, out_size);            return;
    default:                    kbo_build_mod_info_hub_text(out, out_size);             return;
    }
}
/* ---- native\src\hotkey_window\content_refresh.inc ---- */
static void kbo_refresh_hotkey_window(void)
{
    if (g_kbo_hotkey_edit == NULL) {
        return;
    }

    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 65536u);
    if (text == NULL) {
        SetWindowTextA(g_kbo_hotkey_edit, "Ultimate KBO\r\n\r\nCould not prepare this page.");
        return;
    }

    kbo_build_hub_window_text(text, 65536u);
    HFONT content_font = (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES)
        ? g_kbo_hub_font_mono : g_kbo_hub_font_body;
    if (content_font != NULL) {
        SendMessageA(g_kbo_hotkey_edit, WM_SETFONT, (WPARAM)content_font, TRUE);
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                SetWindowTextW(g_kbo_hotkey_edit, wide);
            } else {
                SetWindowTextA(g_kbo_hotkey_edit, text);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        } else {
            SetWindowTextA(g_kbo_hotkey_edit, text);
        }
    } else {
        SetWindowTextA(g_kbo_hotkey_edit, text);
    }
    HeapFree(GetProcessHeap(), 0, text);
}
/* ---- native\src\hotkey_window\ui_view_selection.inc ---- */
static RECT kbo_hub_nav_item_rect(int index, int width, int height)
{
    (void)width;
    RECT rect;
    rect.left   = 12 + (g_kbo_hub_skin_article_gap_x / 2);
    rect.right  = rect.left + 156;
    rect.top    = 98 + (index * (34 + g_kbo_hub_skin_article_gap_y));
    rect.bottom = rect.top + 34;
    if (rect.bottom > height - 12) {
        rect.bottom = height - 12;
    }
    return rect;
}

static const char* kbo_hub_current_view_title(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: return kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80",    "SERVICE TEAMS");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "FOREIGN RIGHTS");
    case KBO_HUB_VIEW_ASIAN_QUOTA: return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "FOREIGN PLAYERS");
    case KBO_HUB_VIEW_ASIAN_GAMES: return kbo_hub_text("ASIAN GAMES", "ASIAN GAMES");
    case KBO_HUB_VIEW_UPCOMING_FA:
    case KBO_HUB_VIEW_FA_CASES: return kbo_hub_text("FA", "FA");
    case KBO_HUB_VIEW_MOD_INFO: return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",      "SETTINGS");
    case KBO_HUB_VIEW_REPUTATION: return kbo_hub_text("\xed\x8f\x89\xed\x8c\x90", "REPUTATION");
    default:                    return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    }
}

static const char* kbo_hub_current_view_subtitle(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY:
        return kbo_hub_military_subnav_label(g_kbo_hub_selected_military_subview);
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xed\x8c\x80\xeb\xb3\x84 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4", "Retained foreign player rights by club");
    case KBO_HUB_VIEW_ASIAN_QUOTA:
        return kbo_hub_foreign_subnav_label(g_kbo_hub_selected_foreign_subview);
    case KBO_HUB_VIEW_ASIAN_GAMES:
        switch (g_kbo_hub_selected_agames_subview) {
        case KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS:
            return kbo_hub_text("ASIAN GAMES TOURNAMENTS", "ASIAN GAMES TOURNAMENTS");
        case KBO_HUB_AGAMES_SUBVIEW_SCHEDULE:
            return kbo_hub_text("ASIAN GAMES SCHEDULE", "ASIAN GAMES SCHEDULE");
        case KBO_HUB_AGAMES_SUBVIEW_ROSTER:
            return kbo_hub_text("ASIAN GAMES ROSTER", "ASIAN GAMES ROSTER");
        default:
            return kbo_hub_text("ASIAN GAMES", "ASIAN GAMES");
        }
    case KBO_HUB_VIEW_UPCOMING_FA:
    case KBO_HUB_VIEW_FA_CASES:
        return kbo_hub_fa_subnav_label(g_kbo_hub_selected_fa_subview);
    case KBO_HUB_VIEW_MOD_INFO:
        return kbo_hub_mod_subnav_label(g_kbo_hub_selected_mod_subview);
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8\xec\x99\x80 \xea\xb2\x8c\xec\x9e\x84 \xed\x94\x8c\xeb\xa0\x88\xec\x9d\xb4 \xec\x84\xa4\xec\xa0\x95", "League and gameplay settings");
    case KBO_HUB_VIEW_REPUTATION: return kbo_hub_text("\xec\xb5\x9c\xea\xb7\xbc 5\xeb\x85\x84 \xed\x8f\x89\xed\x8c\x90 \xeb\xb3\x80\xeb\x8f\x99", "Last 5 years of reputation changes");
    default:                    return kbo_hub_text("\xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xeb\x9f\xb0\xed\x83\x80\xec\x9e\x84 \xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c\xeb\xa5\xbc \xed\x91\x9c\xec\x8b\x9c\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",       "Launcher and runtime patch status");
    }
}

static int kbo_hub_league_keeps_allstar_teams(uint32_t league_id)
{
    char league_name[128] = {0};
    kbo_hub_copy_league_display_name_fast(league_id, league_name, sizeof(league_name));
    return kbo_ascii_contains_ignore_case(league_name, "Tournament")
        || kbo_ascii_contains_ignore_case(league_name, "Classic")
        || kbo_ascii_contains_ignore_case(league_name, "Cup")
        || kbo_ascii_contains_ignore_case(league_name, "Qualifier")
        || kbo_ascii_contains_ignore_case(league_name, "World Baseball")
        || kbo_ascii_contains_ignore_case(league_name, "Asian Games");
}

static int kbo_hub_team_name_is_allstar(const char* name)
{
    return kbo_ascii_contains_ignore_case(name, "All-Star")
        || kbo_ascii_contains_ignore_case(name, "All Star")
        || kbo_ascii_contains_ignore_case(name, "Allstars")
        || kbo_ascii_contains_ignore_case(name, "All-Stars")
        || kbo_ascii_contains_ignore_case(name, "All Stars")
        || kbo_ascii_contains_ignore_case(name, "Futures Stars")
        || kbo_ascii_contains_ignore_case(name, "Future Stars");
}

static int kbo_hub_team_hidden_from_dropdown(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (kbo_hub_league_keeps_allstar_teams(league_id)) {
        return 0;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    char display_name[128] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, display_name, sizeof(display_name), NULL);
    if (kbo_hub_team_name_is_allstar(display_name)) {
        return 1;
    }

    static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && kbo_hub_team_name_is_allstar(text)) {
            return 1;
        }
    }

    return 0;
}

static void kbo_hub_ensure_valid_selection(void)
{
    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        g_kbo_hub_selected_league_id = 0;
        g_kbo_hub_selected_team_id   = 0;
        return;
    }

    int has_selected_team   = 0;
    int has_selected_league = 0;
    int has_kbo_league      = 0;
    uint32_t kbo_league_id  = kbo_resolve_kbo_league_id();
    uint32_t first_league_id = 0;
    uint32_t first_team_id   = 0;
    uint32_t first_team_in_selected_league = 0;
    uint32_t first_team_in_kbo_league      = 0;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (kbo_hub_team_hidden_from_dropdown(team)) {
            continue;
        }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t team_id   = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (league_id == 0 || team_id == 0) {
            continue;
        }
        if (first_league_id == 0) { first_league_id = league_id; }
        if (first_team_id   == 0) { first_team_id   = team_id;   }
        if (kbo_league_id != 0u && league_id == kbo_league_id) {
            has_kbo_league = 1;
            if (first_team_in_kbo_league == 0) { first_team_in_kbo_league = team_id; }
        }
        if (league_id == g_kbo_hub_selected_league_id) {
            has_selected_league = 1;
            if (first_team_in_selected_league == 0) { first_team_in_selected_league = team_id; }
            if (team_id == g_kbo_hub_selected_team_id) { has_selected_team = 1; }
        }
    }

    if (!has_selected_league) {
        g_kbo_hub_selected_league_id = has_kbo_league ? kbo_league_id : first_league_id;
        g_kbo_hub_selected_team_id   = 0;
        first_team_in_selected_league = has_kbo_league ? first_team_in_kbo_league : 0;
        for (int32_t i = 0; i < team_count; i++) {
            if (first_team_in_selected_league != 0) {
                break;
            }
            uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                continue;
            }
            uint8_t* team = (uint8_t*)team_ptr;
            if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
                continue;
            }
            if (kbo_hub_team_hidden_from_dropdown(team)) {
                continue;
            }
            if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == g_kbo_hub_selected_league_id) {
                first_team_in_selected_league = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
                break;
            }
        }
    }

    if (!has_selected_team) {
        g_kbo_hub_selected_team_id = first_team_in_selected_league != 0
            ? first_team_in_selected_league : first_team_id;
    }
}

static POINT kbo_hub_dropdown_anchor_point(HWND hwnd, const RECT* rect)
{
    POINT pt = {0, 0};
    if (hwnd != NULL
            && rect != NULL
            && rect->right > rect->left
            && rect->bottom > rect->top) {
        pt.x = rect->left;
        pt.y = rect->bottom;
        ClientToScreen(hwnd, &pt);
        return pt;
    }

    if (GetCursorPos(&pt)) {
        return pt;
    }

    RECT window_rect = {0, 0, 0, 0};
    if (hwnd != NULL && GetWindowRect(hwnd, &window_rect)) {
        pt.x = window_rect.left + 24;
        pt.y = window_rect.top + 64;
    }
    return pt;
}

static void kbo_hub_show_league_dropdown(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t leagues[256] = {0};
    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < 256; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0) { continue; }
        int exists = 0;
        for (int j = 0; j < league_count; j++) {
            if (leagues[j] == league_id) { exists = 1; break; }
        }
        if (!exists) { leagues[league_count++] = league_id; }
    }
    if (league_count <= 0) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    for (int i = 0; i < league_count; i++) {
        char label[128] = {0};
        WCHAR wide_label[160] = {0};
        kbo_hub_copy_league_display_name_fast(leagues[i], label, sizeof(label));
        kbo_utf8_to_wide(label, wide_label, (int)(sizeof(wide_label) / sizeof(wide_label[0])));
        UINT flags = MF_STRING;
        if (leagues[i] == g_kbo_hub_selected_league_id) { flags |= MF_CHECKED; }
        AppendMenuW(menu, flags, 1000u + (UINT)i, wide_label);
    }

    POINT pt = kbo_hub_dropdown_anchor_point(hwnd, &g_kbo_hub_league_dropdown_rect);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= 1000u && command < 1000u + (UINT)league_count) {
        g_kbo_hub_selected_league_id = leagues[command - 1000u];
        g_kbo_hub_selected_team_id   = 0;
        kbo_hub_ensure_valid_selection();
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

static void kbo_hub_show_team_dropdown(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    if (g_kbo_hub_selected_league_id == 0) {
        kbo_hub_ensure_valid_selection();
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t teams[512] = {0};
    int filtered_count = 0;
    for (int32_t i = 0; i < team_count && filtered_count < 512; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != g_kbo_hub_selected_league_id) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id != 0) { teams[filtered_count++] = team_id; }
    }
    if (filtered_count <= 0) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    for (int i = 0; i < filtered_count; i++) {
        char label[128] = {0};
        WCHAR wide_label[160] = {0};
        kbo_hub_copy_team_display_name_by_id(teams[i], label, sizeof(label), NULL);
        kbo_utf8_to_wide(label, wide_label, (int)(sizeof(wide_label) / sizeof(wide_label[0])));
        UINT flags = MF_STRING;
        if (teams[i] == g_kbo_hub_selected_team_id) { flags |= MF_CHECKED; }
        AppendMenuW(menu, flags, 2000u + (UINT)i, wide_label);
    }

    POINT pt = kbo_hub_dropdown_anchor_point(hwnd, &g_kbo_hub_team_dropdown_rect);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= 2000u && command < 2000u + (UINT)filtered_count) {
        g_kbo_hub_selected_team_id = teams[command - 2000u];
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

/* ---- native\src\hotkey_window\ui_foreign_controls.inc ---- */
static int kbo_hub_foreign_rights_ui_selected(void)
{
    return g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
        || (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            && g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS);
}

static uint32_t kbo_parse_foreign_candidate_player_id_from_line(const char* line)
{
    if (line == NULL || (line[0] != ' ' && line[0] != '>')) {
        return 0u;
    }

    const char* p = line + 1;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0' || strncmp(p, "PLAYER", 6) == 0) {
        return 0u;
    }

    const char* number = line + 27;
    while (*number == ' ') {
        number++;
    }
    if (*number < '0' || *number > '9') {
        return 0u;
    }

    unsigned long raw = strtoul(number, NULL, 10);
    if (raw == 0ul || raw > UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)raw;
}

static int kbo_select_foreign_candidate_from_edit_click(HWND edit, LPARAM lparam)
{
    if (edit == NULL || !kbo_hub_foreign_rights_ui_selected()) {
        return 0;
    }

    POINT point = {(int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam)};
    LRESULT char_result = SendMessageA(edit, EM_CHARFROMPOS, 0, (LPARAM)&point);
    int char_index = LOWORD(char_result);
    int line_index = (int)SendMessageA(edit, EM_LINEFROMCHAR, (WPARAM)char_index, 0);
    if (line_index < 0) {
        return 0;
    }

    char line[256] = {0};
    *((WORD*)line) = (WORD)(sizeof(line) - 1);
    LRESULT copied = SendMessageA(edit, EM_GETLINE, (WPARAM)line_index, (LPARAM)line);
    if (copied <= 0) {
        return 0;
    }
    if (copied >= (LRESULT)sizeof(line)) {
        copied = (LRESULT)sizeof(line) - 1;
    }
    line[copied] = '\0';

    uint32_t player_id = kbo_parse_foreign_candidate_player_id_from_line(line);
    if (player_id == 0u) {
        return 0;
    }

    g_kbo_hub_selected_foreign_player_id = player_id;
    append_logf("foreign rights ui: selected player=%u from row=%d", player_id, line_index);
    kbo_refresh_hotkey_window();
    if (g_kbo_hotkey_window != NULL) {
        InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
    }
    return 1;
}

static LRESULT CALLBACK kbo_hotkey_edit_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_LBUTTONDOWN && kbo_select_foreign_candidate_from_edit_click(hwnd, lparam)) {
        return 0;
    }

    if (g_kbo_hotkey_edit_original_proc != NULL) {
        return CallWindowProcA(g_kbo_hotkey_edit_original_proc, hwnd, message, wparam, lparam);
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static void kbo_refresh_foreign_rights_controls(void)
{
    if (g_kbo_foreign_list == NULL) {
        return;
    }

    SendMessageA(g_kbo_foreign_list, LB_RESETCONTENT, 0, 0);

    if (!kbo_hub_foreign_rights_ui_selected() || g_kbo_hub_selected_team_id == 0u) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    int selected_index = -1;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (player_id == 0u
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || (current_team_id != g_kbo_hub_selected_team_id
                    && active_team_id != g_kbo_hub_selected_team_id
                    && !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id))) {
            continue;
        }

        char player_name[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

        char label[192] = {0};
        snprintf(
            label,
            sizeof(label),
            "%-28.28s   ID %-8u   Nation %-4u   %s%s%s",
            player_name,
            player_id,
            nation_id,
            restricted ? "Restricted " : "",
            dfa ? "DFA " : "",
            inj_active ? "Injured" : "Active");

        LRESULT index = SendMessageA(g_kbo_foreign_list, LB_ADDSTRING, 0, (LPARAM)label);
        if (index >= 0) {
            SendMessageA(g_kbo_foreign_list, LB_SETITEMDATA, (WPARAM)index, (LPARAM)player_id);
            if (player_id == g_kbo_hub_selected_foreign_player_id) {
                selected_index = (int)index;
            }
        }
    }

    if (selected_index >= 0) {
        SendMessageA(g_kbo_foreign_list, LB_SETCURSEL, (WPARAM)selected_index, 0);
    } else if (SendMessageA(g_kbo_foreign_list, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageA(g_kbo_foreign_list, LB_SETCURSEL, 0, 0);
        g_kbo_hub_selected_foreign_player_id = (uint32_t)SendMessageA(g_kbo_foreign_list, LB_GETITEMDATA, 0, 0);
    }
}

static void kbo_apply_foreign_rights_button(int retain)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        append_logf("foreign rights ui: %s blocked by window closed", retain ? "RETAIN" : "SKIP");
        return;
    }

    uint32_t player_id = g_kbo_hub_selected_foreign_player_id;
    if (g_kbo_foreign_list != NULL) {
        LRESULT sel = SendMessageA(g_kbo_foreign_list, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR) {
            LRESULT data = SendMessageA(g_kbo_foreign_list, LB_GETITEMDATA, (WPARAM)sel, 0);
            if (data != LB_ERR && data != 0) {
                player_id = (uint32_t)data;
                g_kbo_hub_selected_foreign_player_id = player_id;
            }
        }
    }

    if (player_id != 0u && kbo_append_foreign_waiver_user_decision(g_kbo_hub_selected_team_id, player_id, retain)) {
        append_logf("foreign rights ui: queued %s team=%u player=%u", retain ? "RETAIN" : "SKIP", g_kbo_hub_selected_team_id, player_id);
        process_foreign_waiver_commands();
        kbo_refresh_foreign_rights_controls();
        kbo_refresh_hotkey_window();
        if (g_kbo_hotkey_window != NULL) {
            InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
        }
    }
}



static void kbo_webview_append_player_id_attrs(KboWindowTextBuffer* buffer, uint32_t player_id)
{
    if (buffer == NULL || player_id == 0u) {
        return;
    }
    kbo_window_text_appendf(buffer, " title='");
    kbo_html_append_escaped(buffer, kbo_hub_text("OOTP \xec\x84\xa0\xec\x88\x98 ID", "OOTP player ID"));
    kbo_window_text_appendf(buffer, ": %u' data-player-id='%u'", player_id, player_id);
}

static void kbo_webview_append_player_name_cell(KboWindowTextBuffer* buffer, const char* player_name, uint32_t player_id)
{
    kbo_window_text_appendf(buffer, "<td class='roName'");
    kbo_webview_append_player_id_attrs(buffer, player_id);
    kbo_window_text_appendf(buffer, ">");
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(buffer, "</td>");
}

static void kbo_webview_append_player_name_link_cell(
    KboWindowTextBuffer* buffer,
    const char* player_name,
    uint32_t player_id,
    const char* href_prefix)
{
    kbo_window_text_appendf(buffer, "<td class='roName'");
    kbo_webview_append_player_id_attrs(buffer, player_id);
    kbo_window_text_appendf(buffer, "><a href='%s%u'", href_prefix != NULL ? href_prefix : "#", player_id);
    kbo_webview_append_player_id_attrs(buffer, player_id);
    kbo_window_text_appendf(buffer, ">");
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(buffer, "</a></td>");
}

static void kbo_webview_append_team_cell(KboWindowTextBuffer* buffer, const char* team_text)
{
    kbo_window_text_appendf(buffer, "<td class='team'>");
    kbo_html_append_escaped(buffer, team_text != NULL && team_text[0] != '\0' ? team_text : "-");
    kbo_window_text_appendf(buffer, "</td>");
}

static void kbo_webview_append_roster_top_bar(KboWindowTextBuffer* buffer, const char* right_text)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'>");
    if (right_text != NULL && right_text[0] != '\0') {
        kbo_window_text_appendf(buffer, "<div class='rosterTopText'>");
        kbo_html_append_escaped(buffer, right_text);
        kbo_window_text_appendf(buffer, "</div>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}
/* ---- native\src\hotkey_window\ui_html_helpers\date_format.inc ---- */
static int kbo_hub_days_until_yyyymmdd(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return -1;
    }
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return -1;
    }
    uint32_t today_serial = kbo_date_serial(year, month, day);
    uint32_t end_serial = kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
    if (today_serial == 0u || end_serial == 0u) {
        return -1;
    }
    return end_serial > today_serial ? (int)(end_serial - today_serial) : 0;
}

static const char* kbo_hub_weekday_abbrev(int weekday)
{
    static const char* names[] = { "SUN.", "MON.", "TUE.", "WED.", "THU.", "FRI.", "SAT." };
    if (weekday < 0 || weekday > 6) {
        return "";
    }
    return names[weekday];
}

static const char* kbo_hub_month_abbrev(uint32_t month)
{
    static const char* names[] = {
        "", "JAN.", "FEB.", "MAR.", "APR.", "MAY", "JUN.",
        "JUL.", "AUG.", "SEP.", "OCT.", "NOV.", "DEC."
    };
    if (month < 1u || month > 12u) {
        return "";
    }
    return names[month];
}

static const char* kbo_hub_day_suffix(uint32_t day)
{
    if ((day % 100u) >= 11u && (day % 100u) <= 13u) {
        return "TH";
    }
    switch (day % 10u) {
    case 1u: return "ST";
    case 2u: return "ND";
    case 3u: return "RD";
    default: return "TH";
    }
}

static void kbo_hub_format_ootp_date(uint32_t year, uint32_t month, uint32_t day, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (year < 1900u || month < 1u || month > 12u || day < 1u || day > 31u) {
        snprintf(out, out_size, "DATE UNKNOWN");
        return;
    }

    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        snprintf(out, out_size, "DATE UNKNOWN");
        return;
    }

    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    int weekday = (int)((days_since_1601 + 1ull) % 7ull);
    snprintf(out, out_size, "%s %s %u%s, %u",
        kbo_hub_weekday_abbrev(weekday),
        kbo_hub_month_abbrev(month),
        day,
        kbo_hub_day_suffix(day),
        year);
}

static void kbo_webview_append_image_src(KboWindowTextBuffer* buffer, const char* path);

/* ---- native\src\hotkey_window\ui_html_helpers\nation_helpers.inc ---- */
static const char* kbo_hub_nation_label_for_id(uint32_t nation_id)
{
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: return "Korea";
    case 12u:                        return "Australia";
    case 36u:                        return "Canada";
    case 43u:                        return "Taiwan";
    case 49u:                        return "Cuba";
    case 56u:                        return "Dominican Republic";
    case 98u:                        return "Japan";
    case 124u:                       return "Mexico";
    case 206u:                       return "United States";
    case 210u:                       return "Venezuela";
    default:                         return "Unknown nation";
    }
}

static const char* kbo_hub_nation_abbrev_for_id(uint32_t nation_id)
{
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: return "KOR";
    case 12u:                        return "AUS";
    case 36u:                        return "CAN";
    case 43u:                        return "TPE";
    case 49u:                        return "CUB";
    case 56u:                        return "DOM";
    case 98u:                        return "JPN";
    case 124u:                       return "MEX";
    case 206u:                       return "USA";
    case 210u:                       return "VEN";
    default:                         return "---";
    }
}

static const char* kbo_hub_nation_flag_file_for_id(uint32_t nation_id)
{
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: return "kor.png";
    case 12u:                        return "aus.png";
    case 36u:                        return "can.png";
    case 43u:                        return "tpe.png";
    case 49u:                        return "cub.png";
    case 56u:                        return "dom.png";
    case 98u:                        return "jpn.png";
    case 124u:                       return "mex.png";
    case 206u:                       return "usa.png";
    case 210u:                       return "ven.png";
    default:                         return "unknown.png";
    }
}

static void kbo_webview_append_nation_flag_image(KboWindowTextBuffer* buffer, uint32_t nation_id)
{
    if (buffer == NULL) {
        return;
    }

    char flag_path[MAX_PATH] = {0};
    kbo_hub_nation_flag_asset_path(kbo_hub_nation_flag_file_for_id(nation_id), flag_path, sizeof(flag_path));
    if (flag_path[0] == '\0' || GetFileAttributesA(flag_path) == INVALID_FILE_ATTRIBUTES) {
        kbo_hub_nation_flag_asset_path("unknown.png", flag_path, sizeof(flag_path));
    }

    kbo_window_text_appendf(buffer, "<img class='roNatFlag' alt='");
    kbo_html_append_escaped(buffer, kbo_hub_nation_abbrev_for_id(nation_id));
    kbo_window_text_appendf(buffer, "' title='");
    kbo_html_append_escaped(buffer, kbo_hub_nation_label_for_id(nation_id));
    kbo_window_text_appendf(buffer, " nation#%u' src='", nation_id);
    kbo_webview_append_image_src(buffer, flag_path);
    kbo_window_text_appendf(buffer, "'>");
}

static void kbo_webview_append_nation_flag_cell(KboWindowTextBuffer* buffer, uint32_t nation_id)
{
    kbo_window_text_appendf(buffer, "<td class='flag'>");
    kbo_webview_append_nation_flag_image(buffer, nation_id);
    kbo_window_text_appendf(buffer, "</td>");
}

static void kbo_webview_append_roster_nation_cell(KboWindowTextBuffer* buffer, uint32_t nation_id)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<td class='roNat' title='");
    kbo_html_append_escaped(buffer, kbo_hub_nation_label_for_id(nation_id));
    kbo_window_text_appendf(buffer, " nation#%u'><span class='roNatWrap'>", nation_id);
    kbo_webview_append_nation_flag_image(buffer, nation_id);
    kbo_window_text_appendf(buffer, "<span class='roNatText'>");
    kbo_html_append_escaped(buffer, kbo_hub_nation_abbrev_for_id(nation_id));
    kbo_window_text_appendf(buffer, "</span></span></td>");
}
/* ---- native\src\hotkey_window\ui_html_helpers\uniform_numbers.inc ---- */
typedef struct KboHubUniformNumberEntry {
    uint32_t player_id;
    char number[8];
} KboHubUniformNumberEntry;

#define KBO_HUB_UNIFORM_NUMBER_CACHE_CAP 8192

static KboHubUniformNumberEntry g_kbo_hub_uniform_number_cache[KBO_HUB_UNIFORM_NUMBER_CACHE_CAP];
static int g_kbo_hub_uniform_number_cache_count = 0;
static char g_kbo_hub_uniform_number_cache_save_path[MAX_PATH] = {0};

static int kbo_hub_csv_copy_field(const char* line, int target_index, char* out, size_t out_size)
{
    if (line == NULL || target_index < 0 || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    int field = 0;
    int quoted = 0;
    size_t len = 0;
    for (const char* p = line; ; p++) {
        char ch = *p;
        int end = ch == '\0' || ch == '\r' || ch == '\n';
        if (!end && ch == '"') {
            if (quoted && p[1] == '"') {
                if (field == target_index && len + 1 < out_size) {
                    out[len++] = '"';
                }
                p++;
            } else {
                quoted = !quoted;
            }
            continue;
        }
        if (end || (ch == ',' && !quoted)) {
            if (field == target_index) {
                out[len] = '\0';
                kbo_hub_trim_ascii(out);
                return 1;
            }
            field++;
            len = 0;
            if (end) {
                break;
            }
            continue;
        }
        if (field == target_index && len + 1 < out_size) {
            out[len++] = ch;
        }
    }
    return 0;
}

static void kbo_hub_uniform_number_cache_put(uint32_t player_id, const char* number)
{
    if (player_id == 0u || number == NULL || number[0] == '\0') {
        return;
    }

    char clean[8] = {0};
    snprintf(clean, sizeof(clean), "%s", number);
    kbo_hub_trim_ascii(clean);
    if (clean[0] == '\0') {
        return;
    }

    for (int i = 0; i < g_kbo_hub_uniform_number_cache_count; i++) {
        if (g_kbo_hub_uniform_number_cache[i].player_id == player_id) {
            snprintf(g_kbo_hub_uniform_number_cache[i].number, sizeof(g_kbo_hub_uniform_number_cache[i].number), "%s", clean);
            return;
        }
    }
    if (g_kbo_hub_uniform_number_cache_count >= KBO_HUB_UNIFORM_NUMBER_CACHE_CAP) {
        return;
    }

    KboHubUniformNumberEntry* entry = &g_kbo_hub_uniform_number_cache[g_kbo_hub_uniform_number_cache_count++];
    entry->player_id = player_id;
    snprintf(entry->number, sizeof(entry->number), "%s", clean);
}

static void kbo_hub_load_uniform_numbers_from_csv(const char* path, int id_column, int number_column)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }

    char line[8192];
    while (fgets(line, sizeof(line), file) != NULL) {
        const char* first = line;
        while (*first == ' ' || *first == '\t') {
            first++;
        }
        if (*first == '\0' || *first == '#' || *first == '/' || *first == ';') {
            continue;
        }

        char id_text[32] = {0};
        char number_text[16] = {0};
        if (!kbo_hub_csv_copy_field(line, id_column, id_text, sizeof(id_text))
                || !kbo_hub_csv_copy_field(line, number_column, number_text, sizeof(number_text))) {
            continue;
        }
        if (id_text[0] < '0' || id_text[0] > '9') {
            continue;
        }
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        kbo_hub_uniform_number_cache_put(player_id, number_text);
    }

    fclose(file);
}

static void kbo_hub_load_uniform_numbers_for_current_save(void)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        g_kbo_hub_uniform_number_cache_count = 0;
        g_kbo_hub_uniform_number_cache_save_path[0] = '\0';
        return;
    }
    if (_stricmp(g_kbo_hub_uniform_number_cache_save_path, save_path) == 0) {
        return;
    }

    g_kbo_hub_uniform_number_cache_count = 0;
    snprintf(g_kbo_hub_uniform_number_cache_save_path, sizeof(g_kbo_hub_uniform_number_cache_save_path), "%s", save_path);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\import_export\\kbo_rosters.csv", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 8);
    snprintf(path, sizeof(path), "%s\\import_export\\kbo_rosters.txt", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 8);
    snprintf(path, sizeof(path), "%s\\data\\derived\\matching\\uniform_number_patch_report.csv", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 7);
}

static void kbo_webview_copy_player_uniform_number(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (player_id == 0u) {
        return;
    }

    kbo_hub_load_uniform_numbers_for_current_save();
    for (int i = 0; i < g_kbo_hub_uniform_number_cache_count; i++) {
        if (g_kbo_hub_uniform_number_cache[i].player_id == player_id) {
            snprintf(out, out_size, "%s", g_kbo_hub_uniform_number_cache[i].number);
            return;
        }
    }
}

/* ---- native\src\hotkey_window\ui_html_helpers\candidate_card.inc ---- */
static void kbo_webview_append_candidate_card(
    KboWindowTextBuffer* buffer,
    uint8_t* player,
    uint32_t player_id,
    uint32_t current_team_id,
    uint32_t nation_id,
    const char* flags,
    uint32_t retained_on,
    uint32_t expires_on)
{
    char player_name[96] = {0};
    char team_abbrev[16] = {0};
    char uniform_number[8] = {0};
    char retained_text[16] = {0};
    char expires_text[16] = {0};
    uint16_t age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
        ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
        : 0u;
    kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
    kbo_hub_copy_team_abbrev_by_id(current_team_id, team_abbrev, sizeof(team_abbrev), NULL);
    kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
    kbo_military_format_yyyymmdd(retained_on, retained_text, sizeof(retained_text));
    kbo_military_format_yyyymmdd(expires_on, expires_text, sizeof(expires_text));

    kbo_window_text_appendf(
        buffer,
        "<tr%s><td class='roAction'><span class='rightsActions'>"
        "<a class='rightsAction rightsRelease' title='Release reserve rights' href='kbo://release/%u' data-player='",
        player_id == g_kbo_hub_selected_foreign_player_id ? " class='selected'" : "",
        player_id);
    kbo_html_append_escaped(buffer, player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(
        buffer,
        "'>-</a></span></td><td class='roPo'>%s</td><td class='roNum'>",
        kbo_webview_player_position_label(player, 0u));
    kbo_html_append_escaped(buffer, uniform_number);
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_player_name_link_cell(
        buffer,
        player_name[0] != '\0' ? player_name : "Unknown player",
        player_id,
        "kbo://select/");
    kbo_window_text_appendf(buffer, "<td class='roTeam'>");
    kbo_html_append_escaped(buffer, team_abbrev[0] != '\0' ? team_abbrev : "-");
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_roster_nation_cell(buffer, nation_id);
    if (age > 0u) {
        kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td>", (uint32_t)age);
    } else {
        kbo_window_text_appendf(buffer, "<td class='roAge'></td>");
    }
    kbo_window_text_appendf(buffer, "<td class='roDate'>");
    kbo_html_append_escaped(buffer, retained_text);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
    kbo_html_append_escaped(buffer, expires_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
    kbo_html_append_escaped(buffer, flags != NULL && flags[0] != '\0' ? flags : "Active");
    kbo_window_text_appendf(buffer, "</td></tr>");
}
/* ---- native\src\hotkey_window\ui_html_helpers\image_sources.inc ---- */
static void kbo_webview_append_file_url(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    kbo_window_text_appendf(buffer, "file:///");
    for (const char* p = path; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\\') {
            kbo_window_text_appendf(buffer, "/");
        } else if (ch == ' ') {
            kbo_window_text_appendf(buffer, "%%20");
        } else if (ch == '#') {
            kbo_window_text_appendf(buffer, "%%23");
        } else if (ch == '%') {
            kbo_window_text_appendf(buffer, "%%25");
        } else {
            kbo_window_text_appendf(buffer, "%c", ch);
        }
    }
}

static void kbo_webview_copy_file_url(const char* path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        return;
    }

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;
    kbo_webview_append_file_url(&buffer, path);
}

static const char* kbo_webview_image_mime_for_path(const char* path)
{
    if (path == NULL) {
        return NULL;
    }
    size_t len = strlen(path);
    if (len >= 4 && ascii_equals_ignore_case(path + len - 4, ".png")) {
        return "image/png";
    }
    if (len >= 3 && ascii_equals_ignore_case(path + len - 3, ".oi")) {
        return "image/png";
    }
    if (len >= 4 && ascii_equals_ignore_case(path + len - 4, ".jpg")) {
        return "image/jpeg";
    }
    if (len >= 5 && ascii_equals_ignore_case(path + len - 5, ".jpeg")) {
        return "image/jpeg";
    }
    return NULL;
}

static void kbo_webview_append_image_src(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    const char* mime = kbo_webview_image_mime_for_path(path);
    if (mime == NULL) {
        kbo_webview_append_file_url(buffer, path);
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > 1024 * 1024) {
        fclose(file);
        return;
    }
    rewind(file);

    unsigned char* data = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)file_size);
    if (data == NULL) {
        fclose(file);
        return;
    }
    size_t read = fread(data, 1, (size_t)file_size, file);
    fclose(file);
    if (read != (size_t)file_size) {
        HeapFree(GetProcessHeap(), 0, data);
        return;
    }

    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    kbo_window_text_appendf(buffer, "data:%s;base64,", mime);
    for (size_t i = 0; i < read; i += 3) {
        unsigned int value = ((unsigned int)data[i]) << 16;
        int remaining = (int)(read - i);
        if (remaining > 1) { value |= ((unsigned int)data[i + 1]) << 8; }
        if (remaining > 2) { value |= ((unsigned int)data[i + 2]); }
        kbo_window_text_appendf(buffer, "%c%c%c%c",
            alphabet[(value >> 18) & 0x3f],
            alphabet[(value >> 12) & 0x3f],
            remaining > 1 ? alphabet[(value >> 6) & 0x3f] : '=',
            remaining > 2 ? alphabet[value & 0x3f] : '=');
    }
    HeapFree(GetProcessHeap(), 0, data);
}

static void kbo_webview_copy_image_src(const char* path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        return;
    }

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;
    kbo_webview_append_image_src(&buffer, path);
    if (out[0] == '\0') {
        kbo_webview_copy_file_url(path, out, out_size);
    }
}

static void kbo_webview_append_dropdown_logo(KboWindowTextBuffer* buffer, const char* path)
{
    kbo_window_text_appendf(buffer, "<span class='ddLogo'>");
    if (path != NULL && path[0] != '\0') {
        kbo_window_text_appendf(buffer, "<img src='");
        kbo_webview_append_image_src(buffer, path);
        kbo_window_text_appendf(buffer, "'>");
    }
    kbo_window_text_appendf(buffer, "</span>");
}

/* ---- native\src\hotkey_window\ui_html_helpers\dropdowns.inc ---- */
static void kbo_webview_append_league_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year)
{
    if (buffer == NULL) {
        return;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t leagues[256] = {0};
    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < 256; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0) { continue; }
        int exists = 0;
        for (int j = 0; j < league_count; j++) {
            if (leagues[j] == league_id) { exists = 1; break; }
        }
        if (!exists) { leagues[league_count++] = league_id; }
    }
    if (league_count <= 0) {
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='dropdown leagueMenu'>");
    for (int i = 0; i < league_count; i++) {
        char label[128] = {0};
        char logo_path[MAX_PATH] = {0};
        kbo_hub_copy_league_display_name_fast(leagues[i], label, sizeof(label));
        kbo_hub_get_league_logo_path(leagues[i], current_year, logo_path, sizeof(logo_path));
        kbo_window_text_appendf(buffer, "<a class='ddItem%s' href='kbo://setleague/%u'>",
            leagues[i] == g_kbo_hub_selected_league_id ? " selected" : "", leagues[i]);
        kbo_webview_append_dropdown_logo(buffer, logo_path);
        kbo_window_text_appendf(buffer, "<span class='ddText'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span></a>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}

static void kbo_webview_append_team_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year)
{
    if (buffer == NULL) {
        return;
    }
    if (g_kbo_hub_selected_league_id == 0) {
        kbo_hub_ensure_valid_selection();
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t teams[512] = {0};
    int filtered_count = 0;
    for (int32_t i = 0; i < team_count && filtered_count < 512; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != g_kbo_hub_selected_league_id) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id != 0) { teams[filtered_count++] = team_id; }
    }
    if (filtered_count <= 0) {
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='dropdown teamMenu'>");
    for (int i = 0; i < filtered_count; i++) {
        char label[128] = {0};
        char logo_path[MAX_PATH] = {0};
        kbo_hub_copy_team_display_name_by_id(teams[i], label, sizeof(label), NULL);
        kbo_hub_get_team_logo_path(teams[i], current_year, logo_path, sizeof(logo_path));
        kbo_window_text_appendf(buffer, "<a class='ddItem%s' href='kbo://setteam/%u'>",
            teams[i] == g_kbo_hub_selected_team_id ? " selected" : "", teams[i]);
        kbo_webview_append_dropdown_logo(buffer, logo_path);
        kbo_window_text_appendf(buffer, "<span class='ddText'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span></a>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_military_parts\military_roster_view.inc ---- */
static void kbo_webview_append_military_roster_view(KboWindowTextBuffer* buffer)
{
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "Sangmu Baseball Team");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "Korean Police Baseball Team");

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, NULL);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable serviceRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roLeague' data-sort-type='text'>League</th><th class='roClub' data-sort-type='text'>Original Club</th><th class='roReturn' data-sort-type='text'>Return Date</th></tr></thead><tbody>");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int rendered = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        for (int32_t i = 0; i < player_count && rendered < 500; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) { continue; }

            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            const char* service_fallback = NULL;
            uint32_t service_team_id = 0;
            if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
                service_team_id  = sang_id;
                service_fallback = sang_name;
            } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
                service_team_id  = kpb_id;
                service_fallback = kpb_name;
            }
            if (service_team_id == 0) { continue; }

            uint32_t original_team_id = 0u;
            uint32_t original_league_id = 0u;
            kbo_military_resolve_original_team(
                player,
                service_team_id,
                sang_id,
                kpb_id,
                &original_team_id,
                &original_league_id);
            kbo_military_repair_original_team_memory(
                player,
                original_team_id,
                original_league_id,
                service_team_id,
                sang_id,
                kpb_id);

            char player_name[96] = {0};
            char uniform_number[8] = {0};
            char service_team_name[64] = {0};
            char original_team_name[64] = {0};
            char return_date[16] = {0};
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
            kbo_hub_copy_team_display_name_by_id(service_team_id,  service_team_name,  sizeof(service_team_name),  service_fallback);
            kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);
            kbo_military_format_yyyymmdd(
                kbo_military_effective_return_yyyymmdd(player),
                return_date,
                sizeof(return_date));

            kbo_window_text_appendf(
                buffer,
                "<tr><td class='roPo'>%s</td><td class='roNum'>",
                kbo_webview_player_position_label(player, 0u));
            kbo_html_append_escaped(buffer, uniform_number);
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", player_id);
            kbo_window_text_appendf(buffer, "<td class='roLeague'>");
            kbo_html_append_escaped(buffer, service_team_name);
            kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
            kbo_html_append_escaped(buffer, original_team_name);
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roReturn'>%s</td></tr>",
                return_date);
            rendered++;
        }
        if (rendered == 0) {
            kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
        }
    } else {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_military_parts\military_applicant_window_helpers.inc ---- */
static int kbo_military_resolve_application_window(
    uint32_t* out_today,
    uint32_t* out_anchor,
    uint32_t* out_announcement)
{
    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uint32_t anchor = kbo_get_latest_offseason_starts_event(today);
    if (anchor == 0u) {
        anchor = kbo_recent_phase_transition_offseason_anchor(league_id, today);
    }
    if (anchor == 0u
            && g_kbo_custom_event_last_offseason_transition_anchor != 0u
            && g_kbo_custom_event_last_offseason_transition_anchor <= today) {
        anchor = g_kbo_custom_event_last_offseason_transition_anchor;
    }
    if (anchor == 0u
            && g_kbo_foreign_priority_last_scheduled_date != 0u
            && g_kbo_foreign_priority_last_scheduled_date <= today) {
        anchor = g_kbo_foreign_priority_last_scheduled_date;
    }

    uint32_t announcement = kbo_add_one_month_yyyymmdd(anchor);
    if (out_today != NULL) { *out_today = today; }
    if (out_anchor != NULL) { *out_anchor = anchor; }
    if (out_announcement != NULL) { *out_announcement = announcement; }
    if (anchor == 0u || announcement == 0u) {
        return 0;
    }
    return today >= anchor && today <= announcement;
}

static int kbo_military_applicant_position_bucket(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 4;
    }
    switch (player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]) {
    case 1u: return 0;
    case 2u: return 1;
    case 3u:
    case 4u:
    case 5u:
    case 6u: return 2;
    case 7u:
    case 8u:
    case 9u: return 3;
    default: break;
    }
    return 4;
}

static void kbo_military_refresh_applicants_for_hotkey_view(void)
{
    uint32_t entry_year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&entry_year, &month, &day)) {
        kbo_current_year_relaxed(&entry_year);
    }
    if (entry_year < 1982u || entry_year > 2300u) {
        return;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint32_t sang_id = sang != NULL && memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    if (sang_id == 0u) {
        return;
    }
    kbo_refresh_military_selection_candidates_from_memory(
        (uint16_t)entry_year,
        sang_id,
        "hotkey_ui_applicants");
}
/* ---- native\src\hotkey_window\ui_html_render\view_military_parts\military_applicant_summary.inc ---- */
static void kbo_webview_append_military_applicant_summary(
    KboWindowTextBuffer* buffer,
    int application_active,
    int pending,
    int pitchers,
    int catchers,
    int infielders,
    int outfielders,
    int other,
    uint32_t anchor,
    uint32_t announcement)
{
    char anchor_text[16] = "-";
    char announcement_text[16] = "-";
    kbo_military_format_yyyymmdd(anchor, anchor_text, sizeof(anchor_text));
    kbo_military_format_yyyymmdd(announcement, announcement_text, sizeof(announcement_text));

    char summary_text[320] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        "View: Applicants - Status: %s - POS: P %d / C %d / INF %d / OF %d / Other %d - %d Applicants - Period: %s to %s",
        application_active ? "Open" : "Closed",
        pitchers,
        catchers,
        infielders,
        outfielders,
        other,
        pending,
        anchor_text,
        announcement_text);
    kbo_webview_append_roster_top_bar(buffer, summary_text);
}
/* ---- native\src\hotkey_window\ui_html_render\view_military_parts\military_applicants_view.inc ---- */
static void kbo_webview_append_military_applicants_view(KboWindowTextBuffer* buffer)
{
    uint32_t today = 0u;
    uint32_t anchor = 0u;
    uint32_t announcement = 0u;
    int application_active = kbo_military_resolve_application_window(&today, &anchor, &announcement);
    if (application_active) {
        kbo_military_refresh_applicants_for_hotkey_view();
    }

    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    int pending = 0;
    int position_counts[5] = {0};
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u) { continue; }
        if (candidate->selected == 0u) {
            pending++;
            uintptr_t player_ptr = candidate->player_ptr;
            if (!kbo_player_pointer_plausible(player_ptr)) {
                player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            }
            int bucket = kbo_military_applicant_position_bucket(
                kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL);
            if (bucket < 0 || bucket > 4) {
                bucket = 4;
            }
            position_counts[bucket]++;
        }
    }

    (void)today;
    int summary_pending = application_active ? pending : 0;
    int summary_pitchers = application_active ? position_counts[0] : 0;
    int summary_catchers = application_active ? position_counts[1] : 0;
    int summary_infielders = application_active ? position_counts[2] : 0;
    int summary_outfielders = application_active ? position_counts[3] : 0;
    int summary_other = application_active ? position_counts[4] : 0;

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights applicantRights'>");
    kbo_webview_append_military_applicant_summary(
        buffer,
        application_active,
        summary_pending,
        summary_pitchers,
        summary_catchers,
        summary_infielders,
        summary_outfielders,
        summary_other,
        anchor,
        announcement);

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable applicantRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roClub' data-sort-type='text'>Original Club</th><th class='roAge' data-sort-type='number'>Age</th>"
        "<th class='roEntry' data-sort-type='number'>Entry Year</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    if (!application_active) {
        kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='6'>지??기간???�닙?�다.</td></tr>");
    }
    for (LONG i = 0; application_active && i < count && rendered < 500; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u) { continue; }
        if (candidate->selected != 0u) { continue; }

        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
        }

        char player_name[96] = {0};
        const char* position_label = "-";
        uint16_t age = 0u;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            position_label = kbo_webview_player_position_label(player, 0u);
            if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
                age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
            }
        } else {
            snprintf(player_name, sizeof(player_name), "#%u", candidate->player_id);
        }

        char original_team_name[64] = {0};
        kbo_hub_copy_team_display_name_by_id(candidate->original_team_id, original_team_name, sizeof(original_team_name), NULL);
        if (original_team_name[0] == '\0') {
            snprintf(original_team_name, sizeof(original_team_name), "-");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td>", position_label);
        kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", candidate->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, original_team_name);
        if (age > 0u) {
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roAge'>%u</td><td class='roEntry'>%u</td><td class='roStatus'>Queued</td></tr>",
                (uint32_t)age,
                (uint32_t)candidate->entry_year);
        } else {
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roAge'></td><td class='roEntry'>%u</td><td class='roStatus'>Queued</td></tr>",
                (uint32_t)candidate->entry_year);
        }
        rendered++;
    }

    if (application_active && rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_military_parts\military_results_view.inc ---- */
static void kbo_webview_append_military_results_view(KboWindowTextBuffer* buffer)
{
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "Sangmu Baseball Team");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "Korean Police Baseball Team");

    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    uint16_t years[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS] = {0};
    int year_count = 0;
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id != 0u && candidate->selected != 0u) {
            uint16_t entry_year = candidate->entry_year;
            int exists = 0;
            for (int y = 0; y < year_count; y++) {
                if (years[y] == entry_year) {
                    exists = 1;
                    break;
                }
            }
            if (!exists && year_count < OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
                years[year_count++] = entry_year;
            }
        }
    }
    for (int left = 0; left < year_count; left++) {
        for (int right = left + 1; right < year_count; right++) {
            if (years[right] > years[left]) {
                uint16_t tmp = years[left];
                years[left] = years[right];
                years[right] = tmp;
            }
        }
    }

    uint32_t selected_year = g_kbo_hub_selected_military_results_year;
    int selected_year_found = 0;
    for (int y = 0; y < year_count; y++) {
        if ((uint32_t)years[y] == selected_year) {
            selected_year_found = 1;
            break;
        }
    }
    if (year_count > 0 && !selected_year_found) {
        selected_year = (uint32_t)years[0];
        g_kbo_hub_selected_military_results_year = selected_year;
    } else if (year_count == 0 && selected_year == 0u) {
        uint32_t current_year = 0u;
        if (kbo_current_year_relaxed(&current_year) && current_year != 0u) {
            selected_year = current_year;
            g_kbo_hub_selected_military_results_year = selected_year;
        }
    }

    int selected_in_year = 0;
    if (selected_year != 0u) {
        for (LONG i = 0; i < count; i++) {
            KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
            if (candidate->player_id != 0u
                    && candidate->selected != 0u
                    && (uint32_t)candidate->entry_year == selected_year) {
                selected_in_year++;
            }
        }
    }

    char summary_text[160] = {0};
    if (selected_year != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Announcement Results - %u - Accepted: %d",
            selected_year,
            selected_in_year);
    } else {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Announcement Results - Accepted: %d",
            selected_in_year);
    }
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>");
    kbo_html_append_escaped(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "</div><div class='rosterTopControls'><span class='rosterTopLabel'>YEAR:</span>"
        "<select class='ootpSelect rosterYearSelect' onchange=\"if(this.value){location.href='kbo://military/results/year/'+this.value}\">");
    if (year_count > 0) {
        for (int y = 0; y < year_count; y++) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' %s>%u</option>",
                (uint32_t)years[y],
                (uint32_t)years[y] == selected_year ? "selected" : "",
                (uint32_t)years[y]);
        }
    } else {
        if (selected_year != 0u) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' selected>%u</option>",
                selected_year,
                selected_year);
        } else {
            kbo_window_text_appendf(buffer, "<option value='' selected>-</option>");
        }
    }
    kbo_window_text_appendf(buffer, "</select></div></div>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable resultRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roClub' data-sort-type='text'>Original Club</th><th class='roLeague' data-sort-type='text'>Service Team</th>"
        "<th class='roReturn' data-sort-type='text'>Return Date</th><th class='roResult' data-sort-type='text'>Result</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (LONG i = 0; i < count && rendered < 500; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u
                || candidate->selected == 0u
                || (uint32_t)candidate->entry_year != selected_year) {
            continue;
        }

        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
        }

        char player_name[96] = {0};
        char original_team_name[64] = {0};
        char service_team_name[64] = {0};
        char return_date[16] = "-";
        const char* position_label = "-";
        snprintf(service_team_name, sizeof(service_team_name), "%s", sang_name[0] != '\0' ? sang_name : "Sangmu Baseball Team");

        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            uint32_t service_team_id = 0;
            const char* service_fallback = NULL;
            if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
                service_team_id = sang_id;
                service_fallback = sang_name;
            } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
                service_team_id = kpb_id;
                service_fallback = kpb_name;
            }
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            if (service_team_id != 0u) {
                kbo_hub_copy_team_display_name_by_id(
                    service_team_id,
                    service_team_name,
                    sizeof(service_team_name),
                    service_fallback);
            }
            position_label = kbo_webview_player_position_label(player, 0u);
            kbo_military_format_yyyymmdd(
                kbo_military_effective_return_yyyymmdd(player),
                return_date,
                sizeof(return_date));
        } else {
            snprintf(player_name, sizeof(player_name), "#%u", candidate->player_id);
        }

        kbo_hub_copy_team_display_name_by_id(candidate->original_team_id, original_team_name, sizeof(original_team_name), NULL);
        if (original_team_name[0] == '\0') {
            snprintf(original_team_name, sizeof(original_team_name), "-");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td>", position_label);
        kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", candidate->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, original_team_name);
        kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
        kbo_html_append_escaped(buffer, service_team_name);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roReturn'>%s</td><td class='roResult'>Accepted</td></tr>",
            return_date);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_military_parts\military_view_router.inc ---- */
static void kbo_webview_append_military_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_RESULTS) {
        kbo_webview_append_military_results_view(buffer);
        return;
    }

    if (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_APPLICANTS) {
        kbo_webview_append_military_applicants_view(buffer);
        return;
    }

    kbo_webview_append_military_roster_view(buffer);
}
/* ---- native\src\hotkey_window\ui_html_render\view_foreign_rights.inc ---- */
static void kbo_webview_append_foreign_rights_view(KboWindowTextBuffer* buffer, const char* window_status)
{
            kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
            kbo_webview_append_roster_top_bar(buffer, window_status);
            kbo_window_text_appendf(
                buffer,
                "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable foreignRightsTable'><thead><tr>"
                "<th class='roAction'>Rights</th><th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th>"
                "<th class='roName' data-sort-type='text'>Name</th><th class='roTeam' data-sort-type='text'>Team</th>"
                "<th class='roNat' data-sort-type='text'>Nationality*</th><th class='roAge' data-sort-type='number'>Age</th>"
                "<th class='roDate' data-sort-type='date'>Retained</th><th class='roDate' data-sort-type='date'>Expires</th>"
                "<th class='roStatus' data-sort-type='text'>Status</th></tr></thead><tbody>");
    
            uintptr_t player_vector = 0;
            int32_t player_count = 0;
            int rendered = 0;
            uint32_t top_player_id = 0;
            uint32_t top_current_team_id = 0;
            uint32_t window_start = 0u;
            uint32_t window_end = 0u;
            uint32_t today = 0u;
            kbo_current_foreign_waiver_window_dates(&window_start, &window_end);
            kbo_get_foreign_waiver_current_yyyymmdd(&today);
            if (g_kbo_hub_selected_foreign_player_id == 0u
                    && kbo_resolve_foreign_waiver_top_candidate_for_team(g_kbo_hub_selected_team_id, &top_player_id, &top_current_team_id)) {
                g_kbo_hub_selected_foreign_player_id = top_player_id;
            }
            if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                for (int32_t i = 0; i < player_count && rendered < 500; i++) {
                    uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
                    if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
                    uint8_t* player = (uint8_t*)player_ptr;
                    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                    if (player_id == 0u || today == 0u
                            || !kbo_has_active_foreign_waiver_right(g_kbo_hub_selected_team_id, player_id, today)
                            || !kbo_player_is_foreign_for_kbo_rights(player)) {
                        continue;
                    }
                    char latest_action[16] = {0};
                    if (kbo_foreign_waiver_latest_decision_action(window_end, g_kbo_hub_selected_team_id, player_id, latest_action, sizeof(latest_action))
                            && _stricmp(latest_action, "SKIP") == 0) {
                        continue;
                    }
                    char flags[96] = {0};
                    snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                        player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
                    if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Reserved"); }
                    uint32_t retained_on = 0u;
                    uint32_t expires_on = 0u;
                    kbo_get_active_foreign_waiver_right_dates(
                        g_kbo_hub_selected_team_id,
                        player_id,
                        today,
                        &retained_on,
                        &expires_on);
                    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
                    kbo_webview_append_candidate_card(buffer, player, player_id, g_kbo_hub_selected_team_id, nation_id, flags, retained_on, expires_on);
                    rendered++;
                }
            }
            if (rendered == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='10'></td></tr>");
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_quota.inc ---- */
static void kbo_webview_append_asian_quota_view(KboWindowTextBuffer* buffer)
{
    int configured_nations = kbo_load_asian_quota_nation_ids_once();
    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);

    char summary_text[256] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        "View: Foreign Players - Raw %u / Asian %u / Non-Asian %u / Effective %u - AQ Nations: %d",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective,
        configured_nations);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable foreignRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roSlot' data-sort-type='text'>Slot</th><th class='roTeam' data-sort-type='text'>Team</th>"
        "<th class='roNat' data-sort-type='text'>Nationality*</th><th class='roAge' data-sort-type='number'>Age</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int rendered = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        for (int32_t i = 0; i < player_count && rendered < 500; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
            if (player_id == 0u
                    || !kbo_player_is_foreign_for_kbo_rights(player)
                    || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
                continue;
            }

            char player_name[96] = {0};
            char uniform_number[8] = {0};
            char current_team_abbrev[16] = {0};
            char flags[96] = {0};
            uint16_t age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
                ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
                : 0u;
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
            kbo_hub_copy_team_abbrev_by_id(current_team_id, current_team_abbrev, sizeof(current_team_abbrev), NULL);
            snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
            if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }

            kbo_window_text_appendf(
                buffer,
                "<tr><td class='roPo'>%s</td><td class='roNum'>",
                kbo_webview_player_position_label(player, 0u));
            kbo_html_append_escaped(buffer, uniform_number);
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", player_id);
            kbo_window_text_appendf(buffer, "<td class='roSlot'>");
            kbo_html_append_escaped(buffer, kbo_hub_foreign_slot_code_for_player(player));
            kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
            kbo_html_append_escaped(buffer, current_team_abbrev[0] != '\0' ? current_team_abbrev : "-");
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_roster_nation_cell(buffer, nation_id);
            if (age > 0u) {
                kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td><td class='roStatus'>", (uint32_t)age);
            } else {
                kbo_window_text_appendf(buffer, "<td class='roAge'></td><td class='roStatus'>");
            }
            kbo_html_append_escaped(buffer, flags);
            kbo_window_text_appendf(buffer, "</td></tr>");
            rendered++;
        }
    }
    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_games_parts\asian_games_schedule_date_helpers.inc ---- */
static int kbo_webview_asian_games_schedule(KboAsianGamesScheduleSeed* out)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        kbo_current_year_relaxed(&year);
    }
    if (year < 1982u || year > 2200u) {
        return 0;
    }
    return kbo_get_next_asian_games_schedule(year, out);
}

static void kbo_webview_format_asian_games_date(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_webview_format_asian_games_date_range(uint32_t start, uint32_t end, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (start == 0u && end == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    if (start == 0u || end == 0u || start == end) {
        kbo_webview_format_asian_games_date(start != 0u ? start : end, out, out_size);
        return;
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    kbo_webview_format_asian_games_date(start, start_text, sizeof(start_text));
    kbo_webview_format_asian_games_date(end, end_text, sizeof(end_text));
    snprintf(out, out_size, "%s - %s", start_text, end_text);
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_games_parts\asian_games_tournament_status.inc ---- */
static const char* kbo_webview_asian_games_tournament_status_class(const KboAsianGamesScheduleSeed* schedule)
{
    if (schedule == NULL || schedule->status[0] == '\0') {
        return "roServing";
    }
    if (ascii_equals_ignore_case(schedule->status, "official")
            || ascii_equals_ignore_case(schedule->status, "confirmed")) {
        return "roReady";
    }
    if (ascii_equals_ignore_case(schedule->status, "provisional")) {
        return "roSoon";
    }
    if (ascii_equals_ignore_case(schedule->status, "projected")) {
        return "roMuted";
    }
    return "roServing";
}

static const char* kbo_webview_asian_games_tournament_phase(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today,
    const char** out_class)
{
    if (out_class != NULL) {
        *out_class = "roServing";
    }
    if (schedule == NULL || schedule->year == 0u || today == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "TBD";
    }
    if (schedule->tournament_start != 0u
            && schedule->tournament_end != 0u
            && today >= schedule->tournament_start
            && today <= schedule->tournament_end) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "In Progress";
    }
    if (schedule->final_date != 0u && today > schedule->final_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Completed";
    }
    if (schedule->selection_date != 0u
            && schedule->tournament_start != 0u
            && today >= schedule->selection_date
            && today < schedule->tournament_start) {
        if (out_class != NULL) { *out_class = "roSoon"; }
        return "Roster Window";
    }
    return "Upcoming";
}

static void kbo_webview_append_asian_games_tournament_row(
    KboWindowTextBuffer* buffer,
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today)
{
    if (buffer == NULL || schedule == NULL || schedule->year == 0u) {
        return;
    }

    char host_text[128] = {0};
    if (schedule->host_city[0] != '\0' && schedule->host_country[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s, %s", schedule->host_city, schedule->host_country);
    } else if (schedule->host_city[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s", schedule->host_city);
    } else if (schedule->host_country[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s", schedule->host_country);
    } else {
        snprintf(host_text, sizeof(host_text), "TBD");
    }

    char tournament_text[40] = {0};
    char selection_text[16] = {0};
    char departure_text[16] = {0};
    char final_text[16] = {0};
    kbo_webview_format_asian_games_date_range(
        schedule->tournament_start,
        schedule->tournament_end,
        tournament_text,
        sizeof(tournament_text));
    kbo_webview_format_asian_games_date(schedule->selection_date, selection_text, sizeof(selection_text));
    kbo_webview_format_asian_games_date(schedule->departure_date, departure_text, sizeof(departure_text));
    kbo_webview_format_asian_games_date(schedule->final_date, final_text, sizeof(final_text));

    const char* phase_class = "";
    const char* phase = kbo_webview_asian_games_tournament_phase(schedule, today, &phase_class);
    const char* status_class = kbo_webview_asian_games_tournament_status_class(schedule);
    const char* status = kbo_asian_games_schedule_status_label(schedule);

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roPo'>%u</td><td class='roName'>",
        schedule->year);
    kbo_html_append_escaped(buffer, host_text);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
    kbo_html_append_escaped(buffer, tournament_text);
    kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
    kbo_html_append_escaped(buffer, selection_text);
    kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
    kbo_html_append_escaped(buffer, departure_text);
    kbo_window_text_appendf(buffer, "</td><td class='roReturn'>");
    kbo_html_append_escaped(buffer, final_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus %s'>", status_class);
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td><td class='roResult %s'>", phase_class);
    kbo_html_append_escaped(buffer, phase);
    kbo_window_text_appendf(buffer, "</td></tr>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_games_parts\asian_games_tournaments_view.inc ---- */
static void kbo_webview_append_asian_games_tournaments_view(KboWindowTextBuffer* buffer)
{
    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        kbo_current_year_relaxed(&current_year);
    }
    if (current_year < 2026u || current_year > 2200u) {
        current_year = 2026u;
    }

    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    KboAsianGamesScheduleSeed schedules[64];
    memset(schedules, 0, sizeof(schedules));
    int count = 0;
    for (uint32_t year = 2026u; year <= 2200u && count < (int)(sizeof(schedules) / sizeof(schedules[0])); year++) {
        KboAsianGamesScheduleSeed schedule;
        if (kbo_get_asian_games_schedule_for_year(year, &schedule)) {
            schedules[count++] = schedule;
        }
    }

    int official_count = 0;
    int provisional_count = 0;
    int projected_count = 0;
    for (int i = 0; i < count; i++) {
        if (ascii_equals_ignore_case(schedules[i].status, "official")
                || ascii_equals_ignore_case(schedules[i].status, "confirmed")) {
            official_count++;
        } else if (ascii_equals_ignore_case(schedules[i].status, "provisional")) {
            provisional_count++;
        } else if (ascii_equals_ignore_case(schedules[i].status, "projected")) {
            projected_count++;
        }
    }

    char summary_text[256] = {0};
    if (count > 0) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Tournaments - %d Listed - Official: %d / Provisional: %d / Projected: %d",
            count,
            official_count,
            provisional_count,
            projected_count);
    } else {
        snprintf(summary_text, sizeof(summary_text), "View: Tournaments - Asian Games");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agTournamentTable'><thead><tr>"
        "<th class='roPo' data-sort-type='number'>Year</th><th class='roName' data-sort-type='text'>Host</th>"
        "<th class='roDate' data-sort-type='date'>Tournament</th><th class='roClub' data-sort-type='date'>Selection</th>"
        "<th class='roTeam' data-sort-type='date'>Departure</th><th class='roReturn' data-sort-type='date'>Final</th>"
        "<th class='roStatus' data-sort-type='text'>Status</th><th class='roResult' data-sort-type='text'>Phase</th>"
        "</tr></thead><tbody>");

    if (count <= 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8' class='roEmptyMessage'></td></tr>");
    } else {
        for (int i = 0; i < count; i++) {
            kbo_webview_append_asian_games_tournament_row(buffer, &schedules[i], today);
        }
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_games_parts\asian_games_schedule_helpers.inc ---- */
static int kbo_webview_weekday_for_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        return -1;
    }
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    return (int)((days_since_1601 + 1ull) % 7ull);
}

static const char* kbo_webview_asian_games_schedule_status(
    uint32_t event_date,
    const char* event_title,
    uint32_t fired_date,
    uint32_t today,
    int event_exists,
    int auto_schedule,
    const char** out_class)
{
    if (event_date == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "TBD";
    }
    int processed = (fired_date == event_date)
        || kbo_custom_event_processed_marker_exists(event_date, event_title);
    if (processed) {
        if (out_class != NULL) { *out_class = "roReady"; }
        return "Complete";
    }
    if (today == event_date) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "Today";
    }
    if (today != 0u && today > event_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Past";
    }
    if (event_exists) {
        if (out_class != NULL) { *out_class = "roServing"; }
        return "Scheduled";
    }
    if (!auto_schedule) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Seeded";
    }
    if (out_class != NULL) { *out_class = "roSoon"; }
    return "Pending";
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_games_parts\asian_games_schedule_view.inc ---- */
static void kbo_webview_append_asian_games_schedule_row(
    KboWindowTextBuffer* buffer,
    uint32_t event_date,
    const char* event_title,
    const char* event_label,
    const char* action_text,
    const char* impact_text,
    uint32_t fired_date,
    uint32_t today,
    uint32_t league_id,
    int auto_schedule)
{
    int event_exists = league_id != 0u && event_date != 0u
        ? kbo_custom_event_exists_by_title_for_date(league_id, event_date, event_title)
        : 0;
    const char* status_class = "";
    const char* status = kbo_webview_asian_games_schedule_status(
        event_date,
        event_title,
        fired_date,
        today,
        event_exists,
        auto_schedule,
        &status_class);

    char date_text[16] = {0};
    const char* weekday = "";
    if (event_date != 0u) {
        snprintf(
            date_text,
            sizeof(date_text),
            "%04u-%02u-%02u",
            event_date / 10000u,
            (event_date / 100u) % 100u,
            event_date % 100u);
        weekday = kbo_hub_weekday_abbrev(kbo_webview_weekday_for_yyyymmdd(event_date));
    } else {
        snprintf(date_text, sizeof(date_text), "TBD");
    }

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roDate'>%s</td><td class='roPo'>%s</td><td class='roName'>",
        date_text,
        weekday);
    kbo_html_append_escaped(buffer, event_label);
    kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
    kbo_html_append_escaped(buffer, action_text);
    kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
    kbo_html_append_escaped(buffer, impact_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus %s'>", status_class);
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_webview_append_asian_games_schedule_view(KboWindowTextBuffer* buffer)
{
    KboAsianGamesScheduleSeed schedule;
    int has_schedule = kbo_webview_asian_games_schedule(&schedule);
    uint32_t schedule_year = has_schedule ? schedule.year : 0u;
    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uint32_t selection_date = has_schedule ? schedule.selection_date : 0u;
    uint32_t departure_date = has_schedule ? schedule.departure_date : 0u;
    uint32_t final_date = has_schedule ? schedule.final_date : 0u;
    int auto_schedule = has_schedule ? kbo_asian_games_schedule_auto_events_enabled(&schedule) : 0;

    int completed = 0;
    if (selection_date != 0u && kbo_custom_event_processed_marker_exists(selection_date, g_kbo_asian_games_selection_event_title)) { completed++; }
    if (departure_date != 0u && kbo_custom_event_processed_marker_exists(departure_date, g_kbo_asian_games_departure_event_title)) { completed++; }
    if (final_date != 0u && kbo_custom_event_processed_marker_exists(final_date, g_kbo_asian_games_final_event_title)) { completed++; }

    char summary_text[256] = {0};
    if (schedule_year != 0u && kbo_asian_games_schedule_has_event_dates(&schedule)) {
        char host_text[128] = {0};
        if (schedule.host_city[0] != '\0' && schedule.host_country[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s, %s", schedule.host_city, schedule.host_country);
        } else if (schedule.host_city[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s", schedule.host_city);
        }
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Schedule - Asian Games %u%s - %s - %d Completed / 3 Events",
            schedule_year,
            host_text,
            kbo_asian_games_schedule_status_label(&schedule),
            completed);
    } else if (schedule_year != 0u) {
        char host_text[128] = {0};
        if (schedule.host_city[0] != '\0' && schedule.host_country[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s, %s", schedule.host_city, schedule.host_country);
        } else if (schedule.host_city[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s", schedule.host_city);
        }
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Schedule - Asian Games %u%s - %s / Dates TBD",
            schedule_year,
            host_text,
            kbo_asian_games_schedule_status_label(&schedule));
    } else {
        snprintf(summary_text, sizeof(summary_text), "View: Schedule - Asian Games");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agScheduleTable'><thead><tr>"
        "<th class='roDate' data-sort-type='date'>Date</th><th class='roPo' data-sort-type='text'>Day</th>"
        "<th class='roName' data-sort-type='text'>Event</th><th class='roLeague' data-sort-type='text'>Action</th>"
        "<th class='roClub' data-sort-type='text'>Impact</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    if (schedule_year == 0u) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6' class='roEmptyMessage'></td></tr>");
    } else {
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            selection_date,
            g_kbo_asian_games_selection_event_title,
            kbo_hub_text("\xeb\x8c\x80\xed\x91\x9c\xed\x8c\x80 \xeb\xb0\x9c\xed\x91\x9c", "Roster Selection"),
            kbo_hub_text("KBO \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0 \xec\x84\xa0\xeb\xb0\x9c", "KBO announces the national-team roster"),
            kbo_hub_text("24\xeb\xaa\x85 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "24-man roster"),
            g_kbo_asian_games_last_selection_fired_date,
            today,
            league_id,
            auto_schedule);
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            departure_date,
            g_kbo_asian_games_departure_event_title,
            kbo_hub_text("\xec\x84\xa0\xec\x88\x98\xeb\x8b\xa8 \xec\xb6\x9c\xea\xb5\xad", "Player Departure"),
            kbo_hub_text("\xec\x84\xa0\xeb\xb0\x9c \xec\x84\xa0\xec\x88\x98 \xea\xb5\xac\xeb\x8b\xa8 \xec\x9d\xb4\xed\x83\x88 \xeb\xb0\x8f \xeb\x8c\x80\xec\xb2\xb4 \xed\x99\x95\xec\x9d\xb8", "Selected players leave their clubs"),
            kbo_hub_text("\xec\xa0\x9c\xed\x95\x9c \xeb\xaa\x85\xeb\x8b\xa8", "Restricted-list window"),
            g_kbo_asian_games_last_departure_fired_date,
            today,
            league_id,
            auto_schedule);
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            final_date,
            g_kbo_asian_games_final_event_title,
            kbo_hub_text("\xea\xb2\xb0\xec\x8a\xb9 / \xeb\xb3\xb5\xea\xb7\x80", "Final / Return"),
            kbo_hub_text("\xeb\x8c\x80\xed\x9a\x8c \xec\xa2\x85\xeb\xa3\x8c \xed\x9b\x84 \xea\xb5\xac\xeb\x8b\xa8 \xeb\xb3\xb5\xea\xb7\x80 \xeb\xb0\x8f \xeb\xb3\x91\xec\x97\xad \xed\x98\x9c\xed\x83\x9d \xec\xb2\x98\xeb\xa6\xac", "Players return after the final"),
            kbo_hub_text("\xea\xb8\x88\xeb\xa9\x94\xeb\x8b\xac \xeb\xa9\xb4\xec\xa0\x9c", "Gold-medal exemption"),
            g_kbo_asian_games_last_final_fired_date,
            today,
            league_id,
            auto_schedule);
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_asian_games_parts\asian_games_view_router.inc ---- */
static void kbo_webview_append_asian_games_view(KboWindowTextBuffer* buffer)
{
    if (g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS) {
        kbo_webview_append_asian_games_tournaments_view(buffer);
        return;
    }
    if (g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_SCHEDULE) {
        kbo_webview_append_asian_games_schedule_view(buffer);
        return;
    }

            kbo_clear_asian_games_roster_if_save_changed("hotkey_ui");
            LONG roster_count = g_kbo_asian_games_roster_count;
            if (roster_count <= 0) {
                kbo_load_asian_games_roster_csv("hotkey_ui");
                roster_count = g_kbo_asian_games_roster_count;
            }
            if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
                roster_count = 0;
            }
            kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
            kbo_webview_append_roster_top_bar(buffer, NULL);
            kbo_window_text_appendf(
                buffer,
                "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agRosterTable'><thead><tr>"
                "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
                "<th class='roLeague' data-sort-type='text'>League</th><th class='roAge' data-sort-type='number'>Age</th><th class='roNat' data-sort-type='text'>Nationality*</th>"
                "<th class='roTeam' data-sort-type='text'>Team</th><th class='roClub' data-sort-type='text'>WC</th></tr></thead><tbody>");
            if (roster_count == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='8'></td></tr>");
            } else {
                for (LONG i = 0; i < roster_count; i++) {
                    KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
                    uintptr_t player_ptr = entry->player_ptr;
                    if (!kbo_player_pointer_plausible(player_ptr)) {
                        player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
                    }
                    char player_name[96] = {0};
                    if (kbo_player_pointer_plausible(player_ptr)) {
                        kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
                    } else {
                        snprintf(player_name, sizeof(player_name), "Unknown player");
                    }
                    char uniform_number[8] = {0};
                    kbo_webview_copy_player_uniform_number(entry->player_id, uniform_number, sizeof(uniform_number));
                    char team_abbrev[16] = {0};
                    kbo_hub_copy_team_abbrev_by_id(entry->original_team_id, team_abbrev, sizeof(team_abbrev), NULL);
                    kbo_window_text_appendf(
                        buffer,
                        "<tr><td class='roPo'>%s</td><td class='roNum'>",
                        kbo_webview_player_position_label(kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL, entry->role));
                    kbo_html_append_escaped(buffer, uniform_number);
                    kbo_window_text_appendf(buffer, "</td>");
                    kbo_webview_append_player_name_cell(buffer, player_name, entry->player_id);
                    kbo_window_text_appendf(
                        buffer,
                        "<td class='roLeague'>Korean National Team</td><td class='roAge'>%u</td>",
                        (uint32_t)entry->age
                    );
                    kbo_webview_append_roster_nation_cell(buffer, OOTP27_KBO_KOREA_NATION_ID);
                    kbo_window_text_appendf(buffer, "<td class='roTeam'>");
                    kbo_html_append_escaped(buffer, team_abbrev);
                    kbo_window_text_appendf(
                        buffer,
                        "</td><td class='roClub'>%s</td></tr>",
                        entry->wildcard ? "YES" : "NO");
                }
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_fa_cases.inc ---- */
static void kbo_webview_append_fa_cases_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    if (rows == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'>Could not allocate classification buffer.</section></div>");
        return;
    }

    KboFaMarketScanSummary summary = {0};
    int count = kbo_collect_fa_market_classifications(
        g_kbo_hub_selected_league_id,
        rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &summary,
        1,
        "f2_webview");

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCases'>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable faCasesTable'><thead><tr>"
        "<th class='roName' data-sort-type='text'>Name</th>"
        "<th data-sort-type='text'>Case</th>"
        "<th data-sort-type='number'>Grade</th>"
        "<th class='roEntry' data-sort-type='number'>Prev Salary</th>"
        "<th data-sort-type='text'>Team</th>"
        "<th data-sort-type='number'>Age</th>"
        "<th data-sort-type='text'>Nation</th>"
        "<th data-sort-type='text'>Rights</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < count && i < 800; i++) {
        KboFaMarketClassification* row = &rows[i];
        char team_abbrev[16] = "-";
        char rights_abbrev[16] = "-";
        char salary_text[32] = "-";
        const char* grade_display = kbo_fa_market_display_grade(row->grade);
        uint32_t grade_sort_rank = kbo_fa_market_display_grade_sort_rank(row->grade);
        kbo_fa_market_format_salary(row->fa_grade_salary, salary_text, sizeof(salary_text));
        kbo_hub_copy_team_abbrev_by_id(
            kbo_fa_market_display_team_id(row),
            team_abbrev,
            sizeof(team_abbrev),
            "-");
        kbo_hub_copy_team_abbrev_by_id(row->rights_team_id, rights_abbrev, sizeof(rights_abbrev), "-");
        kbo_window_text_appendf(buffer, "<tr>");
        kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_case_label(row->case_label));
        kbo_window_text_appendf(buffer, "</td><td data-sort-value='%u'>", grade_sort_rank);
        kbo_html_append_escaped(buffer, grade_display);
        if (row->fa_grade_salary > 0) {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", row->fa_grade_salary);
        } else {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry'>");
        }
        kbo_html_append_escaped(buffer, salary_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td>");
        kbo_html_append_escaped(buffer, team_abbrev);
        kbo_window_text_appendf(
            buffer,
            "</td><td>%u</td><td>",
            (uint32_t)row->age);
        kbo_html_append_escaped(buffer, row->foreign_player ? "Foreign" : "KOR");
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, rights_abbrev);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'>No active players without a current team found.</td></tr>");
    } else if (summary.truncated || count > 800) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'>Output truncated. Open fa_market_classification.csv for the full snapshot.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    HeapFree(GetProcessHeap(), 0, rows);
}
/* ---- native\src\hotkey_window\ui_html_render\view_fa_compensation.inc ---- */
static int kbo_fa_compensation_record_is_final(const KboFaCompensationRecord* rec)
{
    if (rec == NULL) {
        return 0;
    }
    return rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED
        || rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED;
}

static int kbo_fa_compensation_debug_row_count(
    const KboFaCompensationProtectionDebugRow* rows,
    int count,
    uint32_t fa_player_id)
{
    int row_count = 0;
    if (rows == NULL || count <= 0 || fa_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (rows[i].fa_player_id == fa_player_id) {
            row_count++;
        }
    }
    return row_count;
}

static const char* kbo_fa_compensation_decision_label(
    const KboFaCompensationRecord* rec,
    const KboFaCompensationDecisionRow* decision,
    int has_decision)
{
    if (has_decision && decision != NULL) {
        if (strcmp(decision->action, "CASH_ONLY") == 0) {
            return "Cash Only";
        }
        if (decision->selected_player_name[0] != '\0') {
            return decision->selected_player_name;
        }
        return "Player + Cash";
    }
    if (rec != NULL && rec->requires_player_compensation && rec->protect_count > 0u) {
        return "Awaiting list";
    }
    return "Cash Only";
}

static void kbo_webview_append_fa_compensation_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaCompensationProtectionDebugRow* debug_rows = (KboFaCompensationProtectionDebugRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaCompensationProtectionDebugRow));
    if (records == NULL || debug_rows == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'>Could not allocate compensation buffer.</section></div>");
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (debug_rows != NULL) { HeapFree(GetProcessHeap(), 0, debug_rows); }
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int debug_count = kbo_load_fa_compensation_protection_debug_rows(debug_rows, KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    KboFaCompensationRecord* detail_rec = NULL;

    if (g_kbo_hub_selected_fa_compensation_player_id != 0u) {
        for (int i = 0; i < count; i++) {
            if (records[i].player_id == g_kbo_hub_selected_fa_compensation_player_id
                    && records[i].requires_player_compensation
                    && records[i].protect_count > 0u) {
                detail_rec = &records[i];
                break;
            }
        }
    }
    for (int i = 0; i < count && detail_rec == NULL; i++) {
        if (records[i].requires_player_compensation && records[i].protect_count > 0u
                && !kbo_fa_compensation_record_is_final(&records[i])) {
            detail_rec = &records[i];
        }
    }
    for (int i = 0; i < count && detail_rec == NULL; i++) {
        if (records[i].requires_player_compensation && records[i].protect_count > 0u) {
            detail_rec = &records[i];
        }
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCompensation'>");

    if (detail_rec != NULL) {
        KboFaCompensationDecisionRow board_decision;
        int board_has_decision = kbo_load_latest_fa_compensation_decision(detail_rec->player_id, &board_decision);
        int board_detail_rows = kbo_fa_compensation_debug_row_count(debug_rows, debug_count, detail_rec->player_id);
        int board_is_final = kbo_fa_compensation_record_is_final(detail_rec);
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";

        kbo_hub_copy_team_abbrev_by_id(detail_rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(detail_rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(detail_rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)detail_rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)detail_rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, "<section class='faCompBoard'>");
        kbo_window_text_appendf(buffer, "<div class='faCompBoardLead'><div class='faCompBoardTitle'>");
        kbo_html_append_escaped(buffer, detail_rec->player_name);
        kbo_window_text_appendf(buffer, "<span class='faCompBadge'>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_grade(detail_rec->grade));
        kbo_window_text_appendf(buffer, "</span></div><div class='faCompBoardSummary'>");
        if (detail_rec->requires_player_compensation && detail_rec->protect_count > 0u) {
            kbo_window_text_appendf(buffer, "Signing this FA requires cash compensation and one eligible player unless cash-only compensation is selected.");
        } else {
            kbo_window_text_appendf(buffer, "Signing this FA requires cash-only compensation.");
        }
        kbo_window_text_appendf(buffer, "</div></div>");

        kbo_window_text_appendf(buffer, "<div class='faCompBoardPanels'>");

        kbo_window_text_appendf(buffer, "<article class='faCompPanel'><h3>FA PLAYER</h3><dl>");
        kbo_window_text_appendf(buffer, "<dt>Player</dt><dd>");
        kbo_html_append_escaped(buffer, detail_rec->player_name);
        kbo_window_text_appendf(buffer, "</dd><dt>Grade</dt><dd>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_grade(detail_rec->grade));
        kbo_window_text_appendf(buffer, "</dd><dt>Previous Salary</dt><dd>");
        kbo_html_append_escaped(buffer, previous_salary);
        kbo_window_text_appendf(buffer, "</dd><dt>Status</dt><dd>");
        kbo_html_append_escaped(buffer, kbo_fa_compensation_status_label(detail_rec->status));
        kbo_window_text_appendf(buffer, "</dd></dl></article>");

        kbo_window_text_appendf(buffer, "<article class='faCompPanel faCompPanelFocus'><h3>COMPENSATION</h3><dl>");
        kbo_window_text_appendf(buffer, "<dt>Player + Cash</dt><dd>");
        kbo_html_append_escaped(buffer, cash_with_player);
        kbo_window_text_appendf(buffer, "</dd><dt>Cash Only</dt><dd>");
        kbo_html_append_escaped(buffer, cash_only);
        kbo_window_text_appendf(buffer, "</dd><dt>Protected List</dt><dd>%u players</dd><dt>Decision</dt><dd>", detail_rec->protect_count);
        kbo_html_append_escaped(buffer, kbo_fa_compensation_decision_label(detail_rec, &board_decision, board_has_decision));
        kbo_window_text_appendf(buffer, "</dd></dl></article>");

        kbo_window_text_appendf(buffer, "<article class='faCompPanel'><h3>TEAM IMPACT</h3><dl>");
        kbo_window_text_appendf(buffer, "<dt>Original Team</dt><dd>");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</dd><dt>Signing Team</dt><dd>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, "</dd><dt>Board</dt><dd>");
        if (board_is_final) {
            kbo_window_text_appendf(buffer, "Finalized");
        } else if (board_detail_rows <= 0) {
            kbo_window_text_appendf(buffer, "Protected list needed");
        } else {
            kbo_window_text_appendf(buffer, "Ready for decision");
        }
        kbo_window_text_appendf(buffer, "</dd><dt>Route</dt><dd>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, " to ");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</dd></dl></article>");

        kbo_window_text_appendf(buffer, "</div><div class='faCompActionBar'><span>");
        if (board_is_final) {
            kbo_window_text_appendf(buffer, "This compensation case is finalized.");
        } else if (board_detail_rows <= 0) {
            kbo_window_text_appendf(buffer, "Submit the signing team's protected list before selecting compensation.");
        } else {
            kbo_window_text_appendf(buffer, "Choose cash-only compensation or select one available player from the list below.");
        }
        kbo_window_text_appendf(buffer, "</span><div>");
        if (board_is_final) {
            kbo_window_text_appendf(buffer, "<span class='faCompFinal'>Final</span>");
        } else if (board_detail_rows <= 0) {
            kbo_window_text_appendf(buffer, "<a class='rightsTextAction' href='kbo://fa-comp/submit/%u'>Submit List</a>", detail_rec->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<a class='rightsTextAction cashOnly' href='kbo://fa-comp/cash-only/%u'>Cash Only</a>", detail_rec->player_id);
        }
        kbo_window_text_appendf(buffer, "</div></div></section>");
    } else {
        kbo_window_text_appendf(
            buffer,
            "<section class='faCompBoard faCompBoardEmpty'><div class='faCompBoardLead'><div class='faCompBoardTitle'>FA COMPENSATION</div>"
            "<div class='faCompBoardSummary'>No active player-compensation case is ready for review.</div></div></section>");
    }

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap faCompLedgerWrap'><table class='ootpRosterTable faCompTable'><thead><tr>"
        "<th data-sort-type='number'>Date</th>"
        "<th class='roName' data-sort-type='text'>FA</th>"
        "<th data-sort-type='number'>Grade</th>"
        "<th data-sort-type='text'>From</th>"
        "<th data-sort-type='text'>To</th>"
        "<th class='roEntry' data-sort-type='number'>Prev Salary</th>"
        "<th class='roEntry' data-sort-type='number'>Player + Cash</th>"
        "<th class='roEntry' data-sort-type='number'>Cash Only</th>"
        "<th class='roStatus' data-sort-type='text'>Status</th>"
        "<th class='roAction' data-sort-type='text'>Action</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < count && i < 800; i++) {
        KboFaCompensationRecord* rec = &records[i];
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";
        const char* grade_display = kbo_fa_market_display_grade(rec->grade);
        uint32_t grade_sort_rank = kbo_fa_market_display_grade_sort_rank(rec->grade);
        int row_selected = detail_rec != NULL && detail_rec->player_id == rec->player_id;

        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, row_selected ? "<tr class='selected'>" : "<tr>");
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>%u</td>", rec->signed_on_yyyymmdd, rec->signed_on_yyyymmdd);
        kbo_webview_append_player_name_cell(buffer, rec->player_name, rec->player_id);
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>", grade_sort_rank);
        kbo_html_append_escaped(buffer, grade_display);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", rec->previous_salary);
        kbo_html_append_escaped(buffer, previous_salary);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_with_player);
        kbo_html_append_escaped(buffer, cash_with_player);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_only);
        kbo_html_append_escaped(buffer, cash_only);
        kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
        kbo_html_append_escaped(buffer, kbo_fa_compensation_status_label(rec->status));
        kbo_window_text_appendf(buffer, "</td><td class='roAction'>");

        if (rec->requires_player_compensation && rec->protect_count > 0u) {
            if (rec->status == KBO_FA_COMPENSATION_STATUS_PENDING
                    || rec->status == KBO_FA_COMPENSATION_STATUS_RECORDED) {
                kbo_window_text_appendf(buffer, "<a class='rightsTextAction' title='Submit protected list' href='kbo://fa-comp/submit/%u'>Submit</a>", rec->player_id);
            } else {
                kbo_window_text_appendf(buffer, "<a class='rightsTextAction' title='Open compensation board' href='kbo://fa-comp/detail/%u'>Open</a>", rec->player_id);
            }
        } else {
            kbo_html_append_escaped(buffer, "-");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10' class='roEmptyMessage'>No KBO FA compensation obligations recorded.</td></tr>");
    } else if (count > 800) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10' class='roEmptyMessage'>Output truncated. Open fa_compensation.csv for the full ledger.</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");

    if (detail_rec != NULL) {
        KboFaCompensationDecisionRow decision;
        int has_decision = kbo_load_latest_fa_compensation_decision(detail_rec->player_id, &decision);
        int is_final = kbo_fa_compensation_record_is_final(detail_rec);

        kbo_window_text_appendf(
            buffer,
            "<section class='tablewrap rosterTableWrap faCompLists'><table class='ootpRosterTable faCompListTable'><thead><tr>"
            "<th class='faCompPool' data-sort-type='text'>Pool</th>"
            "<th class='faCompRank' data-sort-type='number'>Rank</th>"
            "<th class='roName' data-sort-type='text'>Player</th>"
            "<th class='faCompAge' data-sort-type='number'>Age</th>"
            "<th class='faCompScore' data-sort-type='number'>Score</th>"
            "<th class='roAction' data-sort-type='text'>Decision</th>"
            "</tr></thead><tbody>");

        int rendered = 0;
        static const char* list_order[] = { "auto_protected", "protected", "unprotected" };
        for (int list_index = 0; list_index < 3; list_index++) {
            const char* list_type = list_order[list_index];
            for (int i = 0; i < debug_count; i++) {
                KboFaCompensationProtectionDebugRow* row = &debug_rows[i];
                if (row->fa_player_id != detail_rec->player_id || strcmp(row->list_type, list_type) != 0) {
                    continue;
                }
                const char* label = strcmp(row->list_type, "auto_protected") == 0
                    ? "Auto" : (strcmp(row->list_type, "protected") == 0 ? "Protected" : "Available");
                int selected_player = has_decision
                    && strcmp(decision.action, "PLAYER") == 0
                    && row->player_id == decision.selected_player_id;

                kbo_window_text_appendf(buffer, selected_player ? "<tr class='selected'>" : "<tr>");
                kbo_window_text_appendf(buffer, "<td class='faCompPool'>");
                kbo_html_append_escaped(buffer, label);
                kbo_window_text_appendf(buffer, "</td><td class='faCompRank' data-sort-value='%u'>%u</td>", row->rank, row->rank);
                kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
                kbo_window_text_appendf(
                    buffer,
                    "<td class='faCompAge' data-sort-value='%u'>%u</td><td class='faCompScore' data-sort-value='%d'>%d</td><td class='roAction'>",
                    (uint32_t)row->age,
                    (uint32_t)row->age,
                    row->score,
                    row->score);
                if (selected_player) {
                    kbo_window_text_appendf(buffer, "<span class='faCompPick'>Selected</span>");
                } else if (!is_final && strcmp(row->list_type, "unprotected") == 0) {
                    kbo_window_text_appendf(
                        buffer,
                        "<a class='rightsTextAction' href='kbo://fa-comp/select/%u/%u'>Select</a>",
                        detail_rec->player_id,
                        row->player_id);
                } else {
                    kbo_html_append_escaped(buffer, "-");
                }
                kbo_window_text_appendf(buffer, "</td></tr>");
                rendered++;
            }
        }
        if (rendered == 0) {
            kbo_window_text_appendf(buffer, "<tr><td colspan='6' class='roEmptyMessage'>Protected list has not been submitted yet.</td></tr>");
        }
        kbo_window_text_appendf(buffer, "</tbody></table></section>");
    } else {
        kbo_window_text_appendf(buffer, "<section class='tablewrap rosterTableWrap faCompLists'><table class='ootpRosterTable'><tbody><tr><td class='roEmptyMessage'>No player compensation board is available.</td></tr></tbody></table></section>");
    }

    kbo_window_text_appendf(buffer, "</div>");
    HeapFree(GetProcessHeap(), 0, debug_rows);
    HeapFree(GetProcessHeap(), 0, records);
}
/* ---- native\src\hotkey_window\ui_html_render\view_mod_info_parts\mod_runtime_flags_state.inc ---- */
typedef struct KboModRuntimeFlagSetting {
    const char* key;
    const char* label;
    int enabled_value;
    int default_enabled;
    const char* companion_enable_key;
    int category;
} KboModRuntimeFlagSetting;

enum {
    KBO_MOD_FLAG_USER = 0,
    KBO_MOD_FLAG_RECOVERY = 1,
    KBO_MOD_FLAG_DIAGNOSTIC = 2,
    KBO_MOD_FLAG_LEGACY = 3
};

static const KboModRuntimeFlagSetting KBO_MOD_RUNTIME_FLAG_SETTINGS[] = {
    { "enable_foreign_waiver_ai", "Foreign waiver AI", 1, 1, NULL, KBO_MOD_FLAG_USER },
    { "enable_single_division_allstar_events", "Single-division All-Star events", 1, 1, NULL, KBO_MOD_FLAG_USER },

    { "enable_experimental_runtime_hooks", "Runtime patch engine", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_foreign_waiver_background_scanner", "Foreign waiver background scanner", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_foreign_waiver_legacy_auto_detector", "Foreign waiver legacy detector", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_foreign_trade_check_patch", "Foreign trade quota guard", 0, 1, "enable_kbo_foreign_trade_check_patch", KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_custom_foreign_policy", "Custom foreign policy", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_foreign_injury_replacement", "Foreign injury replacement", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_intl_established_fa_generation_filter", "International FA generation filter", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_fa_compensation", "FA compensation", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_salary_arbitration_no_withdraw_patch", "Salary arbitration no-withdraw", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_no_minor_contract_experimental_patch", "No minor-contract patch", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_sangmu_fa_block_core", "Military team FA block", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_military_team_add_guard_patch", "Military team add guard", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_amateur_assignment_reroute", "Amateur reputation assignment", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_single_division_allstar_runtime_patches", "Single-division All-Star runtime", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_single_division_allstar_voting_hook", "Single-division All-Star voting", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_single_division_allstar_settings_patch", "Single-division All-Star settings", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_fa_salary_opening_day_snapshot", "FA salary opening-day snapshot", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_fa_salary_opening_day_phase_hook", "FA salary phase hook", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_ai_fa_status_candidate_insert_hook", "AI FA candidate hook", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_ai_fa_fallback_patch", "AI FA fallback patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_player_team_signability_patch", "Player-team signability patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_offer_eligibility_patch", "Offer eligibility patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_submit_offer_probe_patch", "Submit-offer probe", 0, 1, "enable_kbo_submit_offer_probe_patch", KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_foreign_signing_branch_patch", "Foreign signing branch hook", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_runtime_roster_marker_guard", "Runtime roster marker guard", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_callup_foreign_limit_patch", "Foreign call-up limit patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_intl_established_fa_quality_probe_patch", "International FA quality probe", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_season_phase_monitor", "Season phase monitor", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },

    { "enable_amateur_assignment_verbose_log", "Amateur assignment verbose log", 1, 0, NULL, KBO_MOD_FLAG_DIAGNOSTIC },

    { "enable_fa_requalification", "FA requalification", 1, 0, NULL, KBO_MOD_FLAG_LEGACY },
    { "enable_kbo_fa_signability_hooks", "Legacy FA signability hooks", 1, 0, NULL, KBO_MOD_FLAG_LEGACY }
};
/* ---- native\src\hotkey_window\ui_html_render\view_mod_info_parts\mod_runtime_flags_access.inc ---- */
static const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
        if (strcmp(KBO_MOD_RUNTIME_FLAG_SETTINGS[i].key, key) == 0) {
            return &KBO_MOD_RUNTIME_FLAG_SETTINGS[i];
        }
    }
    return NULL;
}

static int kbo_get_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = setting->enabled_value ? 0 : 1;
    if (!kbo_read_localappdata_json_flag_value(setting->key, setting->key, &raw_value)) {
        return setting->default_enabled ? 1 : 0;
    }
    return raw_value == setting->enabled_value ? 1 : 0;
}

static int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = enabled ? setting->enabled_value : (setting->enabled_value ? 0 : 1);
    int ok = kbo_write_localappdata_json_int_value(setting->key, raw_value);
    if (ok && enabled && setting->companion_enable_key != NULL && setting->companion_enable_key[0] != '\0') {
        ok = kbo_write_localappdata_json_int_value(setting->companion_enable_key, 1);
    }
    return ok;
}
/* ---- native\src\hotkey_window\ui_html_render\view_mod_info_parts\mod_runtime_flags_render.inc ---- */
static void kbo_webview_append_mod_runtime_flag_row(KboWindowTextBuffer* buffer, const KboModRuntimeFlagSetting* setting)
{
    if (buffer == NULL || setting == NULL || setting->key == NULL || setting->label == NULL) {
        return;
    }
    int enabled = kbo_get_mod_runtime_flag_enabled(setting);
    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow flagSettingRow'><label class='settingLabel' for='flag_%s'>",
        setting->key);
    kbo_html_append_escaped(buffer, setting->label);
    kbo_window_text_appendf(
        buffer,
        "</label><select id='flag_%s' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/flag/%s/'+this.value}\">"
        "<option value='on' %s>On</option>"
        "<option value='off' %s>Off</option>"
        "</select></div>",
        setting->key,
        setting->key,
        enabled ? "selected" : "",
        enabled ? "" : "selected");
}

static void kbo_webview_append_mod_runtime_flag_group(
    KboWindowTextBuffer* buffer,
    int category,
    const char* title,
    const char* help_text)
{
    if (buffer == NULL || title == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<div class='flagGroup'><h3>");
    kbo_html_append_escaped(buffer, title);
    kbo_window_text_appendf(buffer, "</h3>");
    if (help_text != NULL && help_text[0] != '\0') {
        kbo_window_text_appendf(buffer, "<p>");
        kbo_html_append_escaped(buffer, help_text);
        kbo_window_text_appendf(buffer, "</p>");
    }
    for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
        if (KBO_MOD_RUNTIME_FLAG_SETTINGS[i].category == category) {
            kbo_webview_append_mod_runtime_flag_row(buffer, &KBO_MOD_RUNTIME_FLAG_SETTINGS[i]);
        }
    }
    kbo_window_text_appendf(buffer, "</div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_mod_info_parts\mod_info_view.inc ---- */
static void kbo_webview_append_mod_info_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_SETTINGS) {
        int profiler_enabled = kbo_get_profiler_enabled_setting();
        int allow_all_team_actions = kbo_get_allow_all_ui_team_actions_setting();
        kbo_window_text_appendf(
            buffer,
            "<div class='rights settingsGrid'>"
            "<section class='card modCard settingsCard'><h2 class='cardTitle'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\x84\xa4\xec\xa0\x95", "MOD SETTINGS"));
        kbo_window_text_appendf(
            buffer,
            "</h2><div class='settingRow'><label class='settingLabel' for='languageSelect'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("\xed\x91\x9c\xec\x8b\x9c \xec\x96\xb8\xec\x96\xb4", "Display language"));
        kbo_window_text_appendf(
            buffer,
            "</label><select id='languageSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/lang/'+this.value}\">"
            "<option value='ko' %s>%s</option>"
            "<option value='en' %s>English</option>"
            "</select></div>",
            g_kbo_hub_language == KBO_HUB_LANG_KO ? "selected" : "",
            "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",
            g_kbo_hub_language == KBO_HUB_LANG_EN ? "selected" : "");
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='profilerSelect'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("\xed\x94\x84\xeb\xa1\x9c\xed\x8c\x8c\xec\x9d\xbc\xeb\x9f\xac", "Profiler"));
        kbo_window_text_appendf(
            buffer,
            "</label><select id='profilerSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/profiler/'+this.value}\">"
            "<option value='off' %s>Off</option>"
            "<option value='on' %s>On</option>"
            "</select></div>",
            profiler_enabled ? "" : "selected",
            profiler_enabled ? "selected" : "");
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='uiTeamActionsSelect'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("UI \xed\x8c\x80 \xec\x95\xa1\xec\x85\x98", "UI team actions"));
        kbo_window_text_appendf(
            buffer,
            "</label><select id='uiTeamActionsSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/ui-team-actions/'+this.value}\">"
            "<option value='all' %s>%s</option>"
            "<option value='controlled' %s>%s</option>"
            "</select></div>",
            allow_all_team_actions ? "selected" : "",
            kbo_hub_text("\xeb\xaa\xa8\xeb\x93\xa0 \xed\x8c\x80 \xed\x97\x88\xec\x9a\xa9 (\xea\xb0\x9c\xeb\xb0\x9c)", "All teams (development)"),
            allow_all_team_actions ? "" : "selected",
            kbo_hub_text("\xeb\x82\xb4 \xed\x8c\x80\xeb\xa7\x8c \xed\x97\x88\xec\x9a\xa9", "Controlled team only"));
        kbo_window_text_appendf(buffer, "<div class='settingsDivider'></div>");
        kbo_webview_append_mod_runtime_flag_group(
            buffer,
            KBO_MOD_FLAG_USER,
            "User settings",
            "Regular gameplay options intended for normal use.");
        kbo_webview_append_mod_runtime_flag_group(
            buffer,
            KBO_MOD_FLAG_RECOVERY,
            "Recovery switches",
            "Use these when a feature causes errors and you need to keep playing without editing JSON.");
        kbo_webview_append_mod_runtime_flag_group(
            buffer,
            KBO_MOD_FLAG_DIAGNOSTIC,
            "Developer diagnostics",
            "Extra logging and probes for troubleshooting.");
        kbo_webview_append_mod_runtime_flag_group(
            buffer,
            KBO_MOD_FLAG_LEGACY,
            "Legacy compatibility",
            "Older runtime paths kept available while saves and workflows migrate.");
        kbo_window_text_appendf(buffer, "</section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_README) {
        char github_icon_path[MAX_PATH] = {0};
        kbo_hub_local_asset_path("github-mark.png", github_icon_path, sizeof(github_icon_path));

        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>MOD INFO</h2>"
            "<p class='modLead'>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 Out of the Park Baseball 27\xec\x97\x90\xec\x84\x9c KBO\xeb\xa5\xbc \xea\xb0\x80\xec\x9e\xa5 KBO\xeb\x8b\xb5\xea\xb2\x8c \xec\xa6\x90\xea\xb8\xb0\xea\xb8\xb0 \xec\x9c\x84\xed\x95\x9c \xed\x8c\xac \xec\xa0\x9c\xec\x9e\x91 \xeb\xaa\xa8\xeb\x93\x9c\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is a fan-made mod for Out of the Park Baseball 27, built to make the KBO experience feel richer, sharper, and more authentic."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x8b\xa4\xec\xa0\x9c \xeb\xa6\xac\xea\xb7\xb8\xec\x9d\x98 \xed\x9d\x90\xeb\xa6\x84, \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0 \xeb\xa7\xa5\xeb\x9d\xbd, \xed\x95\x9c\xea\xb5\xad \xec\x95\xbc\xea\xb5\xac \xed\x8a\xb9\xec\x9c\xa0\xec\x9d\x98 \xec\xa0\x9c\xeb\x8f\x84\xec\x99\x80 \xeb\xa6\xac\xeb\x93\xac\xec\x9d\x84 \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98 \xea\xb2\x8c\xec\x9e\x84 \xec\x95\x88\xec\x9c\xbc\xeb\xa1\x9c \xec\x9e\x90\xec\x97\xb0\xec\x8a\xa4\xeb\x9f\xbd\xea\xb2\x8c \xeb\x81\x8c\xec\x96\xb4\xec\x98\xa4\xeb\x8a\x94 \xea\xb2\x83\xec\x9d\x84 \xeb\xaa\xa9\xed\x91\x9c\xeb\xa1\x9c \xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Used with the KBO Launcher, it brings launcher-assisted rules, roster context, and Korean baseball rhythms into the game."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "KBO \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98 \xec\x82\xac\xec\x9a\xa9\xed\x95\xa0 \xeb\x95\x8c \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98, \xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80, \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xea\xb0\x99\xec\x9d\x80 \xeb\xb3\xb4\xea\xb0\x95 \xea\xb8\xb0\xeb\x8a\xa5\xec\x9d\xb4 \xeb\xa7\x9e\xeb\xac\xbc\xeb\xa0\xa4 \xea\xb0\x80\xec\x9e\xa5 \xec\x99\x84\xec\x84\xb1\xeb\x90\x9c \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xec\xa0\x9c\xea\xb3\xb5\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "The best experience comes when the mod and launcher work together across foreign-player, military-team, and Asian Games workflows."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 Out of the Park Developments\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88 \xeb\x98\x90\xeb\x8a\x94 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x9c\xb4 \xec\xbd\x98\xed\x85\x90\xec\xb8\xa0\xea\xb0\x80 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is not an official product of, nor affiliated with, Out of the Park Developments."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>GITHUB</h2>"
            "<div class='githubHero'>");
        if (github_icon_path[0] != '\0') {
            kbo_window_text_appendf(buffer, "<img class='githubLogo' src='");
            kbo_webview_append_image_src(buffer, github_icon_path);
            kbo_window_text_appendf(buffer, "'>");
        }
        kbo_window_text_appendf(
            buffer,
            "<div class='githubRepo'><strong>OOTP27_Ultimate_KBO</strong>"
            "<span>github.com/lebronisbest623</span></div></div>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xea\xb0\x9c\xeb\xb0\x9c \xea\xb8\xb0\xeb\xa1\x9d\xea\xb3\xbc \xeb\xb0\xb0\xed\x8f\xac \xed\x9d\x90\xeb\xa6\x84\xec\x9d\x80 GitHub \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xec\x97\x90\xec\x84\x9c \xed\x99\x95\xec\x9d\xb8\xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Development history, releases, and issues are available in the GitHub repository."));
        kbo_window_text_appendf(
            buffer,
            "</p><a class='githubLink' href='kbo://github'>GitHub</a>"
            "</section><section class='card modCard'><h2 class='cardTitle'>SUPPORTED BUILD</h2>"
            "<div class='buildList'>");
        for (size_t i = 0; i < kbo_supported_ootp_build_count(); i++) {
            const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
            if (build == NULL) {
                continue;
            }
            kbo_window_text_appendf(
                buffer,
                "<div class='buildRow'><span class='buildLabel'>%s</span><span class='buildValue'>OOTP 27 %s</span></div>"
                "<div class='buildRow'><span class='buildLabel'>%s</span><span class='buildValue'>0x%08X</span></div>"
                "<div class='buildRow'><span class='buildLabel'>%s</span><span class='buildValue'>0x%08X</span></div>",
                kbo_hub_text("\xeb\xb2\x84\xec\xa0\x84", "Build"),
                build->label,
                kbo_hub_text("\xed\x83\x80\xec\x9e\x84\xec\x8a\xa4\xed\x83\xac\xed\x94\x84", "Timestamp"),
                build->timestamp,
                kbo_hub_text("\xec\x9d\xb4\xeb\xaf\xb8\xec\xa7\x80", "Image"),
                build->size_of_image);
        }
        kbo_window_text_appendf(buffer, "</div></section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_LICENSE) {
        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>LICENSE</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xec\x99\x80 KBO \xeb\x9f\xb0\xec\xb2\x98\xeb\x8a\x94 \xed\x8c\xac \xec\xa0\x9c\xec\x9e\x91 \xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xeb\xa1\x9c, \xea\xb0\x9c\xec\x9d\xb8\xec\xa0\x81\xec\x9d\xb8 OOTP \xed\x94\x8c\xeb\xa0\x88\xec\x9d\xb4\xec\x99\x80 \xec\xbb\xa4\xeb\xae\xa4\xeb\x8b\x88\xed\x8b\xb0 \xed\x99\x9c\xec\x9a\xa9\xec\x9d\x84 \xec\x9c\x84\xed\x95\xb4 \xec\x9e\x88\xeb\x8a\x94 \xea\xb7\xb8\xeb\x8c\x80\xeb\xa1\x9c \xec\xa0\x9c\xea\xb3\xb5\xeb\x90\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO and the KBO Launcher are fan-made project materials provided as-is for personal OOTP play and community use."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xeb\xaa\xa8\xeb\x93\x9c \xed\x8c\x8c\xec\x9d\xbc\xea\xb3\xbc \xeb\x9f\xb0\xec\xb2\x98 \xea\xb5\xac\xec\x84\xb1\xec\x9d\x80 \xec\xb6\x9c\xec\xb2\x98\xeb\xa5\xbc \xeb\x82\xa8\xea\xb8\xb0\xeb\x8a\x94 \xed\x95\x9c \xec\x9e\x90\xec\x9c\xa0\xeb\xa1\xad\xea\xb2\x8c \xec\xb0\xb8\xea\xb3\xa0\xed\x95\x98\xea\xb3\xa0 \xec\x88\x98\xec\xa0\x95\xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4. \xeb\x8b\xa8, \xeb\xb3\xb8 \xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xeb\xa5\xbc \xec\x9c\xa0\xeb\xa3\x8c \xec\x83\x81\xed\x92\x88\xec\x9d\xb4\xeb\x82\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xbd\x98\xed\x85\x90\xec\xb8\xa0\xec\xb2\x98\xeb\x9f\xbc \xeb\xb0\xb0\xed\x8f\xac\xed\x95\x98\xeb\x8a\x94 \xea\xb2\x83\xec\x9d\x80 \xea\xb6\x8c\xec\x9e\xa5\xed\x95\x98\xec\xa7\x80 \xec\x95\x8a\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "You may reference and modify the mod files and launcher configuration as long as attribution remains. Do not sell or present this project as official content."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xeb\xa5\xbc \xec\x9e\xac\xeb\xb0\xb0\xed\x8f\xac\xed\x95\x98\xea\xb1\xb0\xeb\x82\x98 \xec\x88\x98\xec\xa0\x95\xeb\xb3\xb8\xec\x9d\x84 \xea\xb3\xb5\xec\x9c\xa0\xed\x95\xa0 \xeb\x95\x8c\xeb\x8a\x94 Ultimate KBO\xec\x99\x80 \xec\x9b\x90 \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xeb\xa5\xbc \xed\x95\xa8\xea\xbb\x98 \xed\x91\x9c\xec\x8b\x9c\xed\x95\xb4 \xec\xa3\xbc\xec\x84\xb8\xec\x9a\x94.",
                "When redistributing the project or sharing modified versions, keep the Ultimate KBO name and original repository visible."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>NOT OFFICIAL</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 Out of the Park Developments\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88, \xea\xb3\xb5\xec\x8b\x9d \xed\x8c\xa8\xec\xb9\x98, \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x9c\xb4 \xec\xbd\x98\xed\x85\x90\xec\xb8\xa0\xea\xb0\x80 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is not an official product, patch, or affiliated content of Out of the Park Developments."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Out of the Park Baseball, OOTP \xeb\xb0\x8f \xea\xb4\x80\xeb\xa0\xa8 \xec\x83\x81\xed\x91\x9c\xeb\x8a\x94 \xea\xb0\x81 \xea\xb6\x8c\xeb\xa6\xac\xec\x9e\x90\xec\x97\x90\xea\xb2\x8c \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Out of the Park Baseball, OOTP, and related trademarks belong to their respective owners."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>THIRD-PARTY ASSETS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xea\xb5\xac\xeb\x8b\xa8\xeb\xaa\x85, \xeb\xa1\x9c\xea\xb3\xa0, \xeb\xa6\xac\xea\xb7\xb8 \xed\x91\x9c\xec\x8b\x9d \xeb\x93\xb1 \xed\x98\x84\xec\x8b\xa4 \xec\x95\xbc\xea\xb5\xac\xec\x99\x80 \xea\xb4\x80\xeb\xa0\xa8\xeb\x90\x9c \xec\x9e\x90\xec\x82\xb0\xec\x9d\x98 \xea\xb6\x8c\xeb\xa6\xac\xeb\x8a\x94 \xea\xb0\x81 \xea\xb6\x8c\xeb\xa6\xac\xec\x9e\x90\xec\x97\x90\xea\xb2\x8c \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Club names, logos, league marks, and other real-baseball assets belong to their respective rights holders."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x95\xa8\xea\xbb\x98 \xec\xa0\x9c\xea\xb3\xb5\xeb\x90\x98\xeb\x8a\x94 \xed\x8f\xb0\xed\x8a\xb8, \xec\x95\x84\xec\x9d\xb4\xec\xbd\x98, \xeb\x9d\xbc\xec\x9d\xb4\xeb\xb8\x8c\xeb\x9f\xac\xeb\xa6\xac \xeb\x93\xb1\xec\x9d\x80 \xea\xb0\x81\xea\xb0\x81\xec\x9d\x98 \xec\x9b\x90 \xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4 \xec\xa1\xb0\xea\xb1\xb4\xec\x9d\x84 \xeb\x94\xb0\xeb\xa6\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Bundled fonts, icons, libraries, and other third-party materials remain under their original license terms."));
        kbo_window_text_appendf(buffer, "</p></section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CREDITS) {
        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>CREATOR</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xec\x99\x80 KBO \xeb\x9f\xb0\xec\xb2\x98\xeb\x8a\x94 lebronisbest623\xec\x9d\x98 \xec\x9e\x91\xed\x92\x88\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO and the KBO Launcher are works by lebronisbest623."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xeb\xaa\xa8\xeb\x93\x9c \xea\xb5\xac\xec\x84\xb1, \xeb\x9f\xb0\xec\xb2\x98 \xea\xb0\x9c\xeb\xb0\x9c, KBO \xed\x99\x98\xea\xb2\xbd \xea\xb5\xac\xed\x98\x84\xec\x9d\x84 \xed\x95\xa8\xea\xbb\x98 \xeb\x8b\xb4\xec\x95\x98\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "This includes the mod structure, launcher work, and KBO-specific game integration."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>TESTERS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x95\x84\xec\xa7\x81 \xeb\x93\xb1\xeb\xa1\x9d\xeb\x90\x9c \xed\x85\x8c\xec\x8a\xa4\xed\x84\xb0\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "No testers are listed yet."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x85\x8c\xec\x8a\xa4\xed\x8a\xb8\xec\x97\x90 \xed\x95\xa8\xea\xbb\x98\xed\x95\xa0 \xec\x82\xac\xeb\x9e\x8c\xec\x9d\xb4 \xec\x83\x9d\xea\xb8\xb0\xeb\xa9\xb4 \xec\x9d\xb4\xea\xb3\xb3\xec\x97\x90 \xea\xb8\xb0\xeb\xa1\x9d\xeb\x90\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Future testers will be recorded here."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>SPECIAL THANKS</h2>"
            "<p>lazyquokka1218 (facegens &amp; logos)</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x94\x8c\xeb\xa0\x88\xec\x9d\xb4\xec\x96\xb4 facegen\xea\xb3\xbc \xeb\xa1\x9c\xea\xb3\xa0 \xec\x9e\x90\xec\x82\xb0 \xec\x9e\x91\xec\x97\x85\xec\x97\x90 \xea\xb0\x90\xec\x82\xac\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Thank you for the player facegen and logo asset work."));
        kbo_window_text_appendf(buffer, "</p></section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS) {
        char github_icon_path[MAX_PATH] = {0};
        kbo_hub_local_asset_path("github-mark.png", github_icon_path, sizeof(github_icon_path));

        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme modContrib'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>CONTRIBUTIONS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 \xed\x98\xbc\xec\x9e\x90 \xeb\x8b\xab\xec\x95\x84\xeb\x91\x90\xeb\x8a\x94 \xeb\xaa\xa8\xeb\x93\x9c\xea\xb0\x80 \xec\x95\x84\xeb\x8b\x88\xeb\x9d\xbc, KBO\xeb\xa5\xbc \xeb\x8d\x94 \xea\xb7\xb8\xeb\x9f\xb4\xeb\x93\xaf\xed\x95\x98\xea\xb2\x8c \xeb\xa7\x8c\xeb\x93\xa4\xea\xb3\xa0 \xec\x8b\xb6\xec\x9d\x80 \xec\x82\xac\xeb\x9e\x8c\xeb\x93\xa4\xec\x9d\xb4 \xec\xa1\xb0\xea\xb8\x88\xec\x94\xa9 \xeb\xb3\xb4\xed\x83\x9c\xeb\xa9\xb0 \xec\xa2\x8b\xec\x95\x84\xec\xa7\x88 \xec\x88\x98 \xec\x9e\x88\xeb\x8a\x94 \xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is a project that can grow through small contributions from people who want the KBO experience to feel more complete."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "GitHub \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xec\x97\x90\xec\x84\x9c\xeb\x8a\x94 \xeb\xaa\xa8\xeb\x93\x9c \xeb\x8d\xb0\xec\x9d\xb4\xed\x84\xb0, \xea\xb5\xac\xec\x9e\xa5 \xec\x9e\x90\xeb\xa3\x8c, \xeb\xa1\x9c\xea\xb3\xa0, \xed\x8e\x98\xec\x9d\xb4\xec\x8a\xa4\xec\xa0\xa0, \xeb\xb2\x88\xec\x97\xad, \xed\x85\x8c\xec\x8a\xa4\xed\x8a\xb8 \xeb\xa6\xac\xed\x8f\xac\xed\x8a\xb8 \xeb\x93\xb1 \xec\x97\xac\xeb\x9f\xac \xed\x98\x95\xed\x83\x9c\xec\x9d\x98 \xea\xb8\xb0\xec\x97\xac\xeb\xa5\xbc \xeb\xb0\x9b\xec\x9d\x84 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "The GitHub repository can accept many kinds of contributions, including mod data, stadium references, logos, facegens, translations, and testing reports."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x9e\x91\xec\x9d\x80 \xec\x88\x98\xec\xa0\x95 \xec\xa0\x9c\xec\x95\x88, \xeb\x88\x84\xeb\x9d\xbd\xeb\x90\x9c \xec\x9e\x90\xeb\xa3\x8c \xec\xa0\x9c\xeb\xb3\xb4, \xec\x8a\xa4\xed\x81\xac\xeb\xa6\xb0\xec\x83\xb7\xea\xb3\xbc \xec\x9e\xac\xed\x98\x84 \xeb\xa1\x9c\xea\xb7\xb8\xeb\x8f\x84 \xec\xb6\xa9\xeb\xb6\x84\xed\x9e\x88 \xeb\x8f\x84\xec\x9b\x80\xec\x9d\xb4 \xeb\x90\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Small fixes, missing references, screenshots, and reproduction notes are all useful."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>WHAT HELPS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0\xec\x99\x80 \xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95 \xeb\xb3\xb4\xea\xb0\x95, \xea\xb5\xac\xec\x9e\xa5 \xec\xa0\x95\xeb\xb3\xb4, \xed\x8c\x80/\xeb\xa6\xac\xea\xb7\xb8 \xeb\xa1\x9c\xea\xb3\xa0, \xec\x84\xa0\xec\x88\x98 facegen, \xed\x85\x8d\xec\x8a\xa4\xed\x8a\xb8\xec\x99\x80 \xeb\xb2\x88\xec\x97\xad, \xeb\xb2\x84\xea\xb7\xb8 \xeb\xa6\xac\xed\x8f\xac\xed\x8a\xb8\xea\xb0\x80 \xeb\xaa\xa8\xeb\x91\x90 \xea\xb8\xb0\xec\x97\xac \xeb\x8c\x80\xec\x83\x81\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Roster and league tuning, stadium information, team and league logos, player facegens, text, translation, and bug reports are all welcome."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x99\x84\xec\x84\xb1\xeb\x90\x9c \xed\x8c\x8c\xec\x9d\xbc\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x88\xec\x96\xb4\xeb\x8f\x84 \xea\xb4\x9c\xec\xb0\xae\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4. \xec\xb0\xb8\xea\xb3\xa0 \xec\x9e\x90\xeb\xa3\x8c\xeb\x82\x98 \xeb\xb0\xa9\xed\x96\xa5 \xec\xa0\x9c\xec\x95\x88\xeb\xa7\x8c\xec\x9c\xbc\xeb\xa1\x9c\xeb\x8f\x84 \xeb\x8b\xa4\xec\x9d\x8c \xec\x97\x85\xeb\x8d\xb0\xec\x9d\xb4\xed\x8a\xb8\xec\x9d\x98 \xec\x8b\xa4\xeb\xa7\x88\xeb\xa6\xac\xea\xb0\x80 \xeb\x90\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "A contribution does not have to be a finished file. References and direction can still shape the next update."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>GITHUB</h2>");
        if (github_icon_path[0] != '\0') {
            kbo_window_text_appendf(buffer, "<div class='githubHero'><img class='githubLogo' src='");
            kbo_webview_append_image_src(buffer, github_icon_path);
            kbo_window_text_appendf(buffer, "'><div class='githubRepo'><strong>OOTP27_Ultimate_KBO</strong><span>github.com/lebronisbest623</span></div></div>");
        }
        kbo_window_text_appendf(buffer, "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xeb\xa5\xbc \xeb\xb0\xa9\xeb\xac\xb8\xed\x95\xb4 \xec\x9d\xb4\xec\x8a\x88\xeb\xa5\xbc \xeb\x82\xa8\xea\xb8\xb0\xea\xb1\xb0\xeb\x82\x98, \xec\x9e\x90\xeb\xa3\x8c\xeb\xa5\xbc \xea\xb3\xb5\xec\x9c\xa0\xed\x95\x98\xea\xb1\xb0\xeb\x82\x98, \xec\xa7\x81\xec\xa0\x91 \xeb\xb3\x80\xea\xb2\xbd \xec\xa0\x9c\xec\x95\x88\xec\x9d\x84 \xeb\xb3\xb4\xeb\x82\xbc \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Visit the repository to open issues, share material, or send proposed changes."));
        kbo_window_text_appendf(buffer, "</p><a class='githubLink' href='kbo://github'>GitHub</a></section></div>");
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='rights'><section class='card'></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_settings.inc ---- */
static void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    int multiplier = kbo_get_intl_established_fa_multiplier();
    const int presets[] = {1, 2, 5, 10, 20};
    int preset_has_current = 0;
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        if (presets[i] == multiplier) {
            preset_has_current = 1;
            break;
        }
    }

    const char* baseline_labels_ko[9] = {
        "\xec\xb5\x9c\xec\xa0\x80 \xec\x97\xb0\xeb\xb4\x89",
        "\xed\x95\x98\xea\xb8\x89",
        "\xeb\xb3\xb4\xed\x86\xb5 \xec\x9d\xb4\xed\x95\x98",
        "\xed\x8f\x89\xea\xb7\xa0 \xec\x9d\xb4\xed\x95\x98",
        "\xed\x8f\x89\xea\xb7\xa0",
        "\xed\x8f\x89\xea\xb7\xa0 \xec\x9d\xb4\xec\x83\x81",
        "\xec\xa4\x80\xec\xb2\x99\xea\xb8\x89",
        "\xec\x8a\xa4\xed\x83\x80\xea\xb8\x89",
        "\xec\x8a\x88\xed\x8d\xbc\xec\x8a\xa4\xed\x83\x80\xea\xb8\x89"
    };
    const char* baseline_labels_en[9] = {
        "Minimum",
        "Poor",
        "Fair",
        "Below Average",
        "Average",
        "Above Average",
        "Good",
        "Star",
        "Superstar"
    };
    kbo_window_text_appendf(
        buffer,
        "<div class='rights settingsGrid'>"
        "<section class='card modCard settingsCard'><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "LEAGUE SETTINGS"));

    kbo_window_text_appendf(
        buffer,
        "</h2><div class='settingRow'><label class='settingLabel' for='intlFaMultiplierSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 FA \xec\x83\x9d\xec\x84\xb1", "International FA pool"));
    kbo_window_text_appendf(
        buffer,
        "</label><select id='intlFaMultiplierSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://settings/intl-fa-multiplier/'+this.value}\">");
    if (!preset_has_current) {
        kbo_window_text_appendf(
            buffer,
            "<option value='%d' selected>%dx</option>",
            multiplier,
            multiplier);
    }

    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        int value = presets[i];
        kbo_window_text_appendf(
            buffer,
            "<option value='%d' %s>%dx</option>",
            value,
            value == multiplier ? "selected" : "",
            value);
    }

    int quality_cap_enabled = kbo_get_foreign_fa_quality_cap_enabled_setting();
    kbo_window_text_appendf(
        buffer,
        "</select></div><div class='settingRow'><label class='settingLabel' for='foreignFaQualityCapSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x83\x9d\xec\x84\xb1 \xed\x92\x88\xec\xa7\x88 \xec\xba\xa1", "Foreign FA quality cap"));
    kbo_window_text_appendf(
        buffer,
        "</label><select id='foreignFaQualityCapSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://settings/foreign-fa-quality-cap/'+this.value}\">"
        "<option value='on' %s>ON</option>"
        "<option value='off' %s>OFF</option>"
        "</select></div>",
        quality_cap_enabled ? "selected" : "",
        quality_cap_enabled ? "" : "selected");

    kbo_window_text_appendf(
        buffer,
        "<div class='settingsDivider'></div><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x97\xb0\xeb\xb4\x89 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4\xeb\x9d\xbc\xec\x9d\xb8", "FOREIGN PLAYER SALARY BASELINE"));
    kbo_window_text_appendf(buffer, "</h2>");

    for (int i = 0; i < 9; i++) {
        int current = kbo_get_foreign_fa_demand_baseline_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='foreignBaseline%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(baseline_labels_ko[i], baseline_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='foreignBaseline%d' class='ootpSelect salaryInput' type='number' min='0' max='20000000' step='1000' value='%d' "
            "onchange=\"location.href='kbo://settings/foreign-fa-baseline/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            current,
            i);
    }

    kbo_window_text_appendf(buffer, "<div class='settingsDivider'></div><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x84\xec\xbf\xbc\xed\x84\xb0 \xec\x97\xb0\xeb\xb4\x89 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4\xeb\x9d\xbc\xec\x9d\xb8", "ASIAN QUOTA SALARY BASELINE"));
    kbo_window_text_appendf(buffer, "</h2>");

    for (int i = 0; i < 9; i++) {
        int current = kbo_get_asian_quota_fa_demand_baseline_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='asianQuotaBaseline%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(baseline_labels_ko[i], baseline_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='asianQuotaBaseline%d' class='ootpSelect salaryInput' type='number' min='0' max='20000000' step='1000' value='%d' "
            "onchange=\"location.href='kbo://settings/asian-quota-fa-baseline/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            current,
            i);
    }

    kbo_window_text_appendf(buffer, "</section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_reputation.inc ---- */
typedef struct KboHubReputationHistoryRow {
    uint32_t year;
    uint32_t league_id;
    uint32_t team_id;
    int32_t old_reputation;
    int32_t delta;
    int32_t new_reputation;
    int32_t wins;
    int32_t losses;
    int32_t ties;
    int32_t score;
    int32_t rank;
} KboHubReputationHistoryRow;

typedef struct KboHubReputationTeamRow {
    uint32_t team_id;
    char name[128];
    int32_t current_reputation;
    int32_t latest_rank;
    int32_t latest_wins;
    int32_t latest_losses;
    int32_t latest_ties;
    int32_t year_reputation[5];
    int32_t year_delta[5];
    int has_year[5];
} KboHubReputationTeamRow;

static int kbo_hub_compare_reputation_history_rows(const void* a, const void* b)
{
    const KboHubReputationHistoryRow* left = (const KboHubReputationHistoryRow*)a;
    const KboHubReputationHistoryRow* right = (const KboHubReputationHistoryRow*)b;
    if (left->year != right->year) {
        return left->year < right->year ? 1 : -1;
    }
    if (left->rank != right->rank) {
        return left->rank > right->rank ? 1 : -1;
    }
    if (left->new_reputation != right->new_reputation) {
        return left->new_reputation < right->new_reputation ? 1 : -1;
    }
    return 0;
}

static int kbo_hub_compare_reputation_team_rows(const void* a, const void* b)
{
    const KboHubReputationTeamRow* left = (const KboHubReputationTeamRow*)a;
    const KboHubReputationTeamRow* right = (const KboHubReputationTeamRow*)b;
    if (left->current_reputation != right->current_reputation) {
        return left->current_reputation < right->current_reputation ? 1 : -1;
    }
    return _stricmp(left->name, right->name);
}

static int kbo_hub_find_reputation_team_row(KboHubReputationTeamRow* teams, int team_count, uint32_t team_id)
{
    for (int i = 0; i < team_count; i++) {
        if (teams[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_load_reputation_team_rows(uint32_t league_id, KboHubReputationTeamRow* teams, int max_teams)
{
    if (league_id == 0u || teams == NULL || max_teams <= 0) {
        return 0;
    }

    kbo_ensure_amateur_reputation_seeds_loaded();
    int team_count = 0;
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count && team_count < max_teams; i++) {
        KboAmateurReputationSeed* seed = &g_kbo_amateur_reputation_seeds[i];
        if (seed->league_id != league_id || seed->team_id == 0u) {
            continue;
        }
        KboHubReputationTeamRow* team = &teams[team_count++];
        memset(team, 0, sizeof(*team));
        team->team_id = seed->team_id;
        team->current_reputation = seed->reputation;
        team->latest_rank = 9999;
        if (seed->team_name[0] != '\0' && seed->nick_name[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s %s", seed->team_name, seed->nick_name);
        } else if (seed->team_name[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s", seed->team_name);
        } else if (seed->nick_name[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s", seed->nick_name);
        } else if (seed->team_abbr[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s", seed->team_abbr);
        } else {
            snprintf(team->name, sizeof(team->name), "Team %u", seed->team_id);
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    qsort(teams, (size_t)team_count, sizeof(teams[0]), kbo_hub_compare_reputation_team_rows);
    return team_count;
}

static int kbo_hub_load_reputation_history_rows(
    uint32_t league_id,
    KboHubReputationHistoryRow* rows,
    int max_rows,
    uint32_t* selected_years,
    int* out_selected_year_count)
{
    if (out_selected_year_count != NULL) {
        *out_selected_year_count = 0;
    }
    if (league_id == 0u || rows == NULL || max_rows <= 0 || selected_years == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_amateur_reputation_history_path(path, sizeof(path))) {
        return 0;
    }

    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_amateur_reputation_seed_file(path, &buffer, &size) || buffer == NULL) {
        return 0;
    }

    int row_count = 0;
    uint32_t years[64] = {0};
    int year_count = 0;
    char* cursor = buffer;
    while (cursor != NULL && *cursor != '\0') {
        char* line = cursor;
        char* newline = strpbrk(cursor, "\r\n");
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
            while (*cursor == '\r' || *cursor == '\n') {
                cursor++;
            }
        } else {
            cursor = NULL;
        }

        if (line[0] == '\0' || line[0] == '#' || strncmp(line, "year,", 5) == 0) {
            continue;
        }

        const char* cell_cursor = line;
        char cell[128] = {0};
        KboHubReputationHistoryRow row;
        memset(&row, 0, sizeof(row));
        for (int col = 0; col < 12; col++) {
            kbo_amateur_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            switch (col) {
            case 0: row.year = kbo_amateur_reputation_parse_u32(cell); break;
            case 1: row.league_id = kbo_amateur_reputation_parse_u32(cell); break;
            case 2: row.team_id = kbo_amateur_reputation_parse_u32(cell); break;
            case 3: row.old_reputation = (int32_t)strtol(cell, NULL, 10); break;
            case 4: row.delta = (int32_t)strtol(cell, NULL, 10); break;
            case 5: row.new_reputation = (int32_t)strtol(cell, NULL, 10); break;
            case 6: row.wins = (int32_t)strtol(cell, NULL, 10); break;
            case 7: row.losses = (int32_t)strtol(cell, NULL, 10); break;
            case 8: row.ties = (int32_t)strtol(cell, NULL, 10); break;
            case 9: row.score = (int32_t)strtol(cell, NULL, 10); break;
            case 10: row.rank = (int32_t)strtol(cell, NULL, 10); break;
            default: break;
            }
        }

        if (row.league_id != league_id || row.year == 0u || row.team_id == 0u) {
            continue;
        }
        if (row_count < max_rows) {
            rows[row_count++] = row;
        }
        int exists = 0;
        for (int i = 0; i < year_count; i++) {
            if (years[i] == row.year) {
                exists = 1;
                break;
            }
        }
        if (!exists && year_count < (int)(sizeof(years) / sizeof(years[0]))) {
            years[year_count++] = row.year;
        }
    }
    HeapFree(GetProcessHeap(), 0, buffer);

    for (int i = 0; i < year_count; i++) {
        for (int j = i + 1; j < year_count; j++) {
            if (years[i] < years[j]) {
                uint32_t tmp = years[i];
                years[i] = years[j];
                years[j] = tmp;
            }
        }
    }
    int selected_count = year_count < 5 ? year_count : 5;
    for (int i = 0; i < selected_count; i++) {
        selected_years[i] = years[i];
    }
    if (out_selected_year_count != NULL) {
        *out_selected_year_count = selected_count;
    }
    qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_hub_compare_reputation_history_rows);
    return row_count;
}

static void kbo_webview_append_reputation_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboHubReputationTeamRow teams[512];
    KboHubReputationHistoryRow rows[4096];
    uint32_t selected_years[5] = {0};
    int selected_year_count = 0;
    int team_count = kbo_hub_load_reputation_team_rows(
        g_kbo_hub_selected_league_id,
        teams,
        (int)(sizeof(teams) / sizeof(teams[0])));
    int row_count = kbo_hub_load_reputation_history_rows(
        g_kbo_hub_selected_league_id,
        rows,
        (int)(sizeof(rows) / sizeof(rows[0])),
        selected_years,
        &selected_year_count);

    for (int i = 0; i < row_count; i++) {
        KboHubReputationHistoryRow* row = &rows[i];
        int team_index = kbo_hub_find_reputation_team_row(teams, team_count, row->team_id);
        if (team_index < 0) {
            continue;
        }
        for (int y = 0; y < selected_year_count; y++) {
            if (row->year == selected_years[y]) {
                teams[team_index].year_reputation[y] = row->new_reputation;
                teams[team_index].year_delta[y] = row->delta;
                teams[team_index].has_year[y] = 1;
                if (y == 0) {
                    teams[team_index].latest_rank = row->rank;
                    teams[team_index].latest_wins = row->wins;
                    teams[team_index].latest_losses = row->losses;
                    teams[team_index].latest_ties = row->ties;
                }
                break;
            }
        }
    }

    kbo_window_text_appendf(
        buffer,
        "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'><table class='ootpRosterTable reputationTable'><thead><tr>"
        "<th data-sort-type='text'>Team</th><th style='width:88px' data-sort-type='number'>Current</th>"
        "<th style='width:64px' data-sort-type='number'>Rank</th><th style='width:92px' data-sort-type='text'>Record</th>");
    for (int i = 0; i < selected_year_count; i++) {
        kbo_window_text_appendf(buffer, "<th style='width:96px' data-sort-type='number'>%u</th>", selected_years[i]);
    }
    kbo_window_text_appendf(buffer, "</tr></thead><tbody>");

    for (int i = 0; i < team_count; i++) {
        KboHubReputationTeamRow* team = &teams[i];
        kbo_window_text_appendf(
            buffer,
            "<tr><td class='pname'>");
        kbo_html_append_escaped(buffer, team->name);
        kbo_window_text_appendf(
            buffer,
            "</td><td data-sort-value='%d'>%d</td><td data-sort-value='%d'>",
            team->current_reputation,
            team->current_reputation,
            team->latest_rank >= 9999 ? 9999 : team->latest_rank);
        if (team->latest_rank >= 9999) {
            kbo_window_text_appendf(buffer, "-");
        } else {
            kbo_window_text_appendf(buffer, "%d", team->latest_rank);
        }
        kbo_window_text_appendf(
            buffer,
            "</td><td data-sort-value='%03d-%03d-%03d'>%d-%d-%d</td>",
            team->latest_wins,
            team->latest_losses,
            team->latest_ties,
            team->latest_wins,
            team->latest_losses,
            team->latest_ties);
        for (int y = 0; y < selected_year_count; y++) {
            if (!team->has_year[y]) {
                kbo_window_text_appendf(buffer, "<td data-sort-value='-1'>-</td>");
                continue;
            }
            const char* delta_class = team->year_delta[y] > 0 ? "pos" : (team->year_delta[y] < 0 ? "neg" : "even");
            kbo_window_text_appendf(
                buffer,
                "<td class='%s' data-sort-value='%d'>%d (%+d)</td>",
                delta_class,
                team->year_reputation[y],
                team->year_reputation[y],
                team->year_delta[y]);
        }
        kbo_window_text_appendf(buffer, "</tr>");
    }

    if (team_count == 0) {
        kbo_window_text_appendf(
            buffer,
            "<tr><td colspan='4' class='roEmptyMessage'>No reputation seed is available for this league.</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_fallback.inc ---- */
static void kbo_webview_append_fallback_text_view(KboWindowTextBuffer* buffer)
{
            char text[32768] = {0};
            kbo_build_hub_window_text(text, sizeof(text));
            kbo_window_text_appendf(buffer, "<div class='card'><pre style=\"white-space:pre-wrap;margin:0;font-family:'Malgun Gothic';line-height:1.55\">");
            kbo_html_append_escaped(buffer, text);
            kbo_window_text_appendf(buffer, "</pre></div>");
}
/* ---- native\src\hotkey_window\ui_html_render\view_router.inc ---- */
static void kbo_webview_append_selected_view(KboWindowTextBuffer* buffer, uint32_t current_year, const char* window_status)
{
    (void)current_year;
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO) {
        kbo_webview_append_mod_info_view(buffer);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        kbo_webview_append_military_view(buffer);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
        kbo_webview_append_foreign_rights_view(buffer, window_status);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        if (g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS) {
            kbo_webview_append_foreign_rights_view(buffer, window_status);
        } else {
            kbo_webview_append_asian_quota_view(buffer);
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        kbo_webview_append_asian_games_view(buffer);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES) {
        if (g_kbo_hub_selected_fa_subview == KBO_HUB_FA_SUBVIEW_COMPENSATION) {
            kbo_webview_append_fa_compensation_view(buffer);
        } else {
            kbo_webview_append_fa_cases_view(buffer);
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS) {
        kbo_webview_append_settings_view(buffer);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_REPUTATION) {
        kbo_webview_append_reputation_view(buffer);
    } else {
        kbo_webview_append_fallback_text_view(buffer);
    }
}
/* ---- native\src\hotkey_window\ui_html_render\render_tabs.inc ---- */
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

static int kbo_hub_selected_league_is_kbo(void)
{
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id != 0u && g_kbo_hub_selected_league_id == kbo_league_id;
}

static int kbo_hub_selected_league_is_amateur_reputation_league(void)
{
    return g_kbo_hub_selected_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
        || g_kbo_hub_selected_league_id == KBO_COLLEGE_LEAGUE_ID;
}

static int kbo_hub_view_available_for_selected_league(int view)
{
    if (view == KBO_HUB_VIEW_MOD_INFO) {
        return 1;
    }
    if (view == KBO_HUB_VIEW_REPUTATION) {
        return kbo_hub_selected_league_is_amateur_reputation_league();
    }
    return kbo_hub_selected_league_is_kbo();
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
        KBO_HUB_VIEW_SETTINGS,
        KBO_HUB_VIEW_REPUTATION
    };
    for (size_t i = 0; i < sizeof(main_views) / sizeof(main_views[0]); i++) {
        int view = main_views[i];
        if (!kbo_hub_view_available_for_selected_league(view)) {
            continue;
        }
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
/* ---- native\src\hotkey_window\ui_html_render\scrollbar_skin_css.inc ---- */
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
        ".dropdown::-webkit-scrollbar:vertical,.content::-webkit-scrollbar:vertical,.rights::-webkit-scrollbar:vertical,.settingsGrid::-webkit-scrollbar:vertical,.settingsCard::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:var(--ootp-sb-width)!important}"
        ".dropdown::-webkit-scrollbar-track:vertical,.content::-webkit-scrollbar-track:vertical,.rights::-webkit-scrollbar-track:vertical,.settingsGrid::-webkit-scrollbar-track:vertical,.settingsCard::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background-color:#101010!important;background-image:var(--ootp-sb-bar-top),var(--ootp-sb-bar-bottom),var(--ootp-sb-bar-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical,.content::-webkit-scrollbar-thumb:vertical,.rights::-webkit-scrollbar-thumb:vertical,.settingsGrid::-webkit-scrollbar-thumb:vertical,.settingsCard::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px;background-color:#2a2a2a!important;background-image:var(--ootp-sb-slider-up-top),var(--ootp-sb-slider-up-bottom),var(--ootp-sb-slider-up-mid)!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:hover,.content::-webkit-scrollbar-thumb:vertical:hover,.rights::-webkit-scrollbar-thumb:vertical:hover,.settingsGrid::-webkit-scrollbar-thumb:vertical:hover,.settingsCard::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:var(--ootp-sb-slider-over-top),var(--ootp-sb-slider-over-bottom),var(--ootp-sb-slider-over-mid)!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:active,.content::-webkit-scrollbar-thumb:vertical:active,.rights::-webkit-scrollbar-thumb:vertical:active,.settingsGrid::-webkit-scrollbar-thumb:vertical:active,.settingsCard::-webkit-scrollbar-thumb:vertical:active,.tablewrap::-webkit-scrollbar-thumb:vertical:active,.card::-webkit-scrollbar-thumb:vertical:active{background-image:var(--ootp-sb-slider-down-top),var(--ootp-sb-slider-down-bottom),var(--ootp-sb-slider-down-mid)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement,.content::-webkit-scrollbar-button:vertical:decrement,.rights::-webkit-scrollbar-button:vertical:decrement,.settingsGrid::-webkit-scrollbar-button:vertical:decrement,.settingsCard::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-less-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment,.content::-webkit-scrollbar-button:vertical:increment,.rights::-webkit-scrollbar-button:vertical:increment,.settingsGrid::-webkit-scrollbar-button:vertical:increment,.settingsCard::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:var(--ootp-sb-width)!important;background-color:#101010!important;background-image:var(--ootp-sb-more-up)!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:hover,.content::-webkit-scrollbar-button:vertical:decrement:hover,.rights::-webkit-scrollbar-button:vertical:decrement:hover,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:hover,.settingsCard::-webkit-scrollbar-button:vertical:decrement:hover,.tablewrap::-webkit-scrollbar-button:vertical:decrement:hover,.card::-webkit-scrollbar-button:vertical:decrement:hover{background-image:var(--ootp-sb-less-over)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:hover,.content::-webkit-scrollbar-button:vertical:increment:hover,.rights::-webkit-scrollbar-button:vertical:increment:hover,.settingsGrid::-webkit-scrollbar-button:vertical:increment:hover,.settingsCard::-webkit-scrollbar-button:vertical:increment:hover,.tablewrap::-webkit-scrollbar-button:vertical:increment:hover,.card::-webkit-scrollbar-button:vertical:increment:hover{background-image:var(--ootp-sb-more-over)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:active,.content::-webkit-scrollbar-button:vertical:decrement:active,.rights::-webkit-scrollbar-button:vertical:decrement:active,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:active,.settingsCard::-webkit-scrollbar-button:vertical:decrement:active,.tablewrap::-webkit-scrollbar-button:vertical:decrement:active,.card::-webkit-scrollbar-button:vertical:decrement:active{background-image:var(--ootp-sb-less-down)!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:active,.content::-webkit-scrollbar-button:vertical:increment:active,.rights::-webkit-scrollbar-button:vertical:increment:active,.settingsGrid::-webkit-scrollbar-button:vertical:increment:active,.settingsCard::-webkit-scrollbar-button:vertical:increment:active,.tablewrap::-webkit-scrollbar-button:vertical:increment:active,.card::-webkit-scrollbar-button:vertical:increment:active{background-image:var(--ootp-sb-more-down)!important}",
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
        ".dropdown::-webkit-scrollbar:vertical,.content::-webkit-scrollbar:vertical,.rights::-webkit-scrollbar:vertical,.settingsGrid::-webkit-scrollbar:vertical,.settingsCard::-webkit-scrollbar:vertical,.tablewrap::-webkit-scrollbar:vertical,.card::-webkit-scrollbar:vertical{width:%dpx!important;background:transparent!important}"
        ".dropdown::-webkit-scrollbar:horizontal,.content::-webkit-scrollbar:horizontal,.rights::-webkit-scrollbar:horizontal,.settingsGrid::-webkit-scrollbar:horizontal,.settingsCard::-webkit-scrollbar:horizontal,.tablewrap::-webkit-scrollbar:horizontal,.card::-webkit-scrollbar:horizontal{display:none!important;height:0!important}"
        ".dropdown::-webkit-scrollbar-track:vertical,.content::-webkit-scrollbar-track:vertical,.rights::-webkit-scrollbar-track:vertical,.settingsGrid::-webkit-scrollbar-track:vertical,.settingsCard::-webkit-scrollbar-track:vertical,.tablewrap::-webkit-scrollbar-track:vertical,.card::-webkit-scrollbar-track:vertical{background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical,.content::-webkit-scrollbar-thumb:vertical,.rights::-webkit-scrollbar-thumb:vertical,.settingsGrid::-webkit-scrollbar-thumb:vertical,.settingsCard::-webkit-scrollbar-thumb:vertical,.tablewrap::-webkit-scrollbar-thumb:vertical,.card::-webkit-scrollbar-thumb:vertical{min-height:42px!important;background-color:transparent!important;background-image:url('%s'),url('%s'),url('%s')!important;background-repeat:no-repeat,no-repeat,repeat-y!important;background-position:center top,center bottom,center top!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:hover,.content::-webkit-scrollbar-thumb:vertical:hover,.rights::-webkit-scrollbar-thumb:vertical:hover,.settingsGrid::-webkit-scrollbar-thumb:vertical:hover,.settingsCard::-webkit-scrollbar-thumb:vertical:hover,.tablewrap::-webkit-scrollbar-thumb:vertical:hover,.card::-webkit-scrollbar-thumb:vertical:hover{background-image:url('%s'),url('%s'),url('%s')!important}"
        ".dropdown::-webkit-scrollbar-thumb:vertical:active,.content::-webkit-scrollbar-thumb:vertical:active,.rights::-webkit-scrollbar-thumb:vertical:active,.settingsGrid::-webkit-scrollbar-thumb:vertical:active,.settingsCard::-webkit-scrollbar-thumb:vertical:active,.tablewrap::-webkit-scrollbar-thumb:vertical:active,.card::-webkit-scrollbar-thumb:vertical:active{background-image:url('%s'),url('%s'),url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement,.content::-webkit-scrollbar-button:vertical:decrement,.rights::-webkit-scrollbar-button:vertical:decrement,.settingsGrid::-webkit-scrollbar-button:vertical:decrement,.settingsCard::-webkit-scrollbar-button:vertical:decrement,.tablewrap::-webkit-scrollbar-button:vertical:decrement,.card::-webkit-scrollbar-button:vertical:decrement{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment,.content::-webkit-scrollbar-button:vertical:increment,.rights::-webkit-scrollbar-button:vertical:increment,.settingsGrid::-webkit-scrollbar-button:vertical:increment,.settingsCard::-webkit-scrollbar-button:vertical:increment,.tablewrap::-webkit-scrollbar-button:vertical:increment,.card::-webkit-scrollbar-button:vertical:increment{height:%dpx!important;background-color:transparent!important;background-image:url('%s')!important;background-repeat:no-repeat!important;background-position:center center!important;border:0!important;box-shadow:none!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:hover,.content::-webkit-scrollbar-button:vertical:decrement:hover,.rights::-webkit-scrollbar-button:vertical:decrement:hover,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:hover,.settingsCard::-webkit-scrollbar-button:vertical:decrement:hover,.tablewrap::-webkit-scrollbar-button:vertical:decrement:hover,.card::-webkit-scrollbar-button:vertical:decrement:hover{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:hover,.content::-webkit-scrollbar-button:vertical:increment:hover,.rights::-webkit-scrollbar-button:vertical:increment:hover,.settingsGrid::-webkit-scrollbar-button:vertical:increment:hover,.settingsCard::-webkit-scrollbar-button:vertical:increment:hover,.tablewrap::-webkit-scrollbar-button:vertical:increment:hover,.card::-webkit-scrollbar-button:vertical:increment:hover{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:decrement:active,.content::-webkit-scrollbar-button:vertical:decrement:active,.rights::-webkit-scrollbar-button:vertical:decrement:active,.settingsGrid::-webkit-scrollbar-button:vertical:decrement:active,.settingsCard::-webkit-scrollbar-button:vertical:decrement:active,.tablewrap::-webkit-scrollbar-button:vertical:decrement:active,.card::-webkit-scrollbar-button:vertical:decrement:active{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-button:vertical:increment:active,.content::-webkit-scrollbar-button:vertical:increment:active,.rights::-webkit-scrollbar-button:vertical:increment:active,.settingsGrid::-webkit-scrollbar-button:vertical:increment:active,.settingsCard::-webkit-scrollbar-button:vertical:increment:active,.tablewrap::-webkit-scrollbar-button:vertical:increment:active,.card::-webkit-scrollbar-button:vertical:increment:active{background-image:url('%s')!important}"
        ".dropdown::-webkit-scrollbar-corner,.content::-webkit-scrollbar-corner,.rights::-webkit-scrollbar-corner,.settingsGrid::-webkit-scrollbar-corner,.settingsCard::-webkit-scrollbar-corner,.tablewrap::-webkit-scrollbar-corner,.card::-webkit-scrollbar-corner{background:transparent!important}",
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
/* ---- native\src\hotkey_window\ui_html_render\roster_table_css.inc ---- */
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
        ".applicantRosterTable .roName{width:230px}.applicantRosterTable .roClub{width:190px}.applicantRosterTable .roStatus{color:#ff7a00;font-weight:500}.resultRosterTable .roName{width:240px}.resultRosterTable .roLeague{width:210px}.resultRosterTable .roResult{color:#ff7a00;font-weight:500}.foreignRosterTable .roName{width:230px}.foreignRosterTable .roNat{width:160px}.foreignRosterTable .roStatus{width:140px}.foreignRosterTable .roSlot{color:#ff7a00;font-weight:500}.foreignRightsTable .roAction{width:58px;text-align:center}.foreignRightsTable .roName{width:230px}.foreignRightsTable .roNat{width:150px}.foreignRightsTable .roStatus{width:140px}.foreignRightsTable .roTeam{width:82px}.faCasesTable .roName{width:220px}.faCasesTable .roStatus{width:340px}.faCompensation{display:flex;flex-direction:column}.faCompLists{margin-top:8px}.faCompListTable .roName{width:230px}.rightsTextAction{display:inline-flex;align-items:center;justify-content:center;min-width:52px;height:21px;padding:0 8px;border:1px solid #494949;border-radius:3px;background:#2a2a2a;color:#f1f1f1;font-size:12px;font-weight:900;text-decoration:none}.rightsTextAction:hover{border-color:#8a8a8a;background:#383838}.faCases .metricRow{display:flex;align-items:center;gap:7px;min-height:28px;padding:0 8px 6px;overflow:hidden}.faCases .pill{display:inline-flex;align-items:center;min-width:0;height:22px;padding:0 8px;border:1px solid #343434;border-radius:4px;background:#202020;color:#e8e8e8;font-family:var(--ui-font);font-size:12px;font-weight:900;white-space:nowrap}.agTournamentTable .roPo{width:60px}.agTournamentTable .roName{width:220px}.agTournamentTable .roDate{width:190px}.agTournamentTable .roClub{width:116px}.agTournamentTable .roTeam{width:116px}.agTournamentTable .roReturn{width:116px}.agTournamentTable .roStatus{width:118px}.agTournamentTable .roResult{width:122px}.rightsActions{display:flex;align-items:center;justify-content:center;min-width:0}.rightsAction{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;border:0;border-radius:0;background-color:transparent;background-repeat:no-repeat;background-position:center center;background-size:24px 24px;color:transparent;font-size:0;line-height:0;text-decoration:none;cursor:pointer}.rightsRelease{background-image:var(--ootp-btn-minus-up)}.rightsRelease:hover{background-image:var(--ootp-btn-minus-over)}.rightsRelease:active{background-image:var(--ootp-btn-minus-down)}.ootpConfirmOverlay{position:fixed;inset:0;z-index:1200;display:none;align-items:center;justify-content:center;background:rgba(0,0,0,.46);font-family:var(--ui-font)}.ootpConfirmOverlay.show{display:flex}.ootpConfirmDialog{width:430px;max-width:calc(100vw - 36px);border:1px solid #383838;border-radius:5px;background:#1a1a1a;box-shadow:0 10px 28px rgba(0,0,0,.72),inset 0 1px 0 rgba(255,255,255,.05);overflow:hidden}.ootpConfirmTitle{height:31px;display:flex;align-items:center;padding:0 11px;background:#2a2a2a;color:#f2f2f2;font-size:14px;font-weight:900;border-bottom:1px solid #0f0f0f;text-transform:uppercase}.ootpConfirmBody{padding:14px 14px 10px;color:#e8e8e8;font-size:13px;font-weight:400;line-height:1.42}.ootpConfirmPlayer{margin-top:8px;padding:7px 9px;border:1px solid #313131;border-radius:3px;background:#101010;color:#fff;font-weight:800;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.ootpConfirmDetail{margin-top:8px;color:#a7a7a7}.ootpConfirmActions{display:flex;justify-content:flex-end;gap:8px;padding:0 14px 14px}.ootpConfirmButton{min-width:96px;height:28px;border:1px solid #414141;border-radius:4px;background:#262626;color:#eeeeee;font-family:var(--ui-font);font-size:13px;font-weight:900}.ootpConfirmButton:hover{border-color:#777;background:#303030}.ootpConfirmButton.primary{background:#3b2318;border-color:#7c3b24;color:#fff}.ootpConfirmButton.primary:hover{background:#562c1d;border-color:#a24d2d}.rosterRights{gap:0!important}.rosterTopBar{height:34px;display:flex;align-items:center;justify-content:flex-end;gap:12px;padding:0 8px 4px 8px;background:transparent;border:0;color:#e8e8e8;font-family:var(--ui-font);font-size:14px;font-weight:700;line-height:1;white-space:nowrap;overflow:hidden}.rosterTopText{margin-left:auto;min-width:0;max-width:100%%;overflow:hidden;text-overflow:ellipsis;text-align:right}.rosterTopControls{display:flex;align-items:center;justify-content:flex-end;gap:8px;flex:none}.rosterTopLabel{color:#f0f0f0;font-size:14px;font-weight:900;text-transform:uppercase}.rosterTopBar .rosterYearSelect{width:96px;height:24px;border:1px solid #c8c8c8;border-radius:4px;background:#ededed;color:#111;font-family:var(--ui-font);font-size:13px;font-weight:900;padding:0 24px 0 9px}.ootpRosterTable .roEmptyMessage{text-align:center;color:#dcdcdc;font-weight:500;background:#202020!important}");
    kbo_window_text_appendf(
        css,
        ".faCompensation{display:grid!important;grid-template-rows:196px minmax(104px,30%%) minmax(160px,1fr);gap:6px;height:100%%;min-height:0;overflow:hidden}"
        ".faCompLedgerWrap,.faCompLists{min-height:0;margin-top:0!important}"
        ".faCompTable .roName{width:190px}.faCompTable .roEntry{width:108px}.faCompTable .roStatus{width:138px}.faCompTable .roAction{width:78px;text-align:center}"
        ".faCompBoard{min-height:0;border:1px solid #171717;border-radius:2px;background:#1b1b1b;box-shadow:inset 0 1px 0 rgba(255,255,255,.04);font-family:var(--ui-font);overflow:hidden}"
        ".faCompBoardLead{height:44px;display:grid;grid-template-columns:minmax(0,1fr);align-content:center;padding:4px 10px;border-bottom:1px solid #282828;background:#202020}"
        ".faCompBoardTitle{display:flex;align-items:center;gap:8px;min-width:0;color:#f2f2f2;font-size:15px;font-weight:900;line-height:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.faCompBoardSummary{min-width:0;color:#bdbdbd;font-size:12px;font-weight:600;line-height:16px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".faCompBoardPanels{height:114px;display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1.15fr) minmax(0,1fr);gap:0;border-bottom:1px solid #282828}.faCompPanel{min-width:0;padding:8px 10px;border-right:1px solid #282828;background:#181818;overflow:hidden}.faCompPanel:last-child{border-right:0}.faCompPanelFocus{background:#1d1d1d}"
        ".faCompPanel h3{margin:0 0 7px;color:#9c9c9c;font-size:13px;font-weight:900;line-height:15px;text-transform:uppercase}.faCompPanel dl{display:grid;grid-template-columns:94px minmax(0,1fr);gap:4px 8px;margin:0;color:#e8e8e8;font-size:12px;line-height:15px}.faCompPanel dt{color:#8f8f8f;font-weight:900;white-space:nowrap}.faCompPanel dd{margin:0;min-width:0;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".faCompActionBar{height:36px;display:grid;grid-template-columns:minmax(0,1fr) auto;align-items:center;gap:10px;padding:0 10px;color:#d8d8d8;font-size:12px;font-weight:700}.faCompActionBar span{min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".faCompBadge{display:inline-flex;align-items:center;justify-content:center;min-width:22px;height:18px;padding:0 5px;border:1px solid #4a4a4a;border-radius:2px;background:#262626;color:#ffb13b;font-size:12px;font-weight:900}"
        ".faCompBoardEmpty{display:flex;align-items:center}.faCompBoardEmpty .faCompBoardLead{width:100%%;border-bottom:0}.faCompListTable .roName{width:260px}.faCompListTable .roAction{width:96px;text-align:center}"
        ".faCompPool{width:92px}.faCompRank{width:58px;text-align:right}.faCompAge{width:52px;text-align:right}.faCompScore{width:88px;text-align:right}.faCompPick{color:#ffb13b;font-weight:900}.faCompFinal{display:inline-flex;align-items:center;justify-content:center;min-width:68px;height:21px;color:#bdbdbd;font-size:12px;font-weight:900}.rightsTextAction.cashOnly{min-width:74px}");
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
/* ---- native\src\hotkey_window\ui_html_render\roster_sort_script.inc ---- */
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
/* ---- native\src\hotkey_window\ui_html_render\hub_html_builder.inc ---- */
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
    if (!kbo_hub_view_available_for_selected_league(g_kbo_hub_selected_view)) {
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        if (g_kbo_hub_selected_mod_subview < 0
                || g_kbo_hub_selected_mod_subview >= KBO_HUB_MOD_SUBVIEW_COUNT) {
            g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_README;
        }
    }

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
        g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_REPUTATION;
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
        ".dropdown{position:absolute;z-index:20;top:72px;max-height:420px;overflow-y:auto;overflow-x:hidden;background:#242424;border:1px solid #333;border-radius:3px;box-shadow:0 8px 18px rgba(0,0,0,.55);padding:4px 0;scrollbar-gutter:stable}.leagueMenu{right:190px;width:304px}.teamMenu{right:12px;width:304px}.ddItem{height:24px;display:flex!important;flex-direction:row!important;align-items:center;justify-content:flex-start;gap:6px;padding:0 10px 0 5px;color:#f2f2f2;font-family:var(--ui-font);font-size:16px;font-weight:800;line-height:24px;white-space:nowrap}.ddItem:hover,.ddItem.selected{background:#30434b}.ddLogo{width:20px;height:24px;display:inline-flex;align-items:center;justify-content:center;flex:0 0 20px;overflow:hidden}.ddLogo img{display:block;width:auto;height:auto;max-width:18px!important;max-height:18px!important;object-fit:contain}.ddText{display:block;flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".panel{margin:18px 16px 16px;background:var(--panel);border:1px solid var(--panel2);border-radius:5px;min-height:0;height:calc(100%% - 34px);display:grid;grid-template-rows:66px 1fr;overflow:hidden}.panelHead{background:var(--panel2);padding:10px 15px}.panelHead h1{margin:0;font-family:var(--ui-font);font-size:%dpx;font-weight:800}.panelHead p{margin:3px 0 0;color:var(--muted)}"
        ".content{padding:16px;min-height:0;height:100%%;display:flex;flex-direction:column;overflow:hidden}.rights,.card{flex:1;min-height:0;height:100%%}.rights{display:flex;flex-direction:column;gap:10px}.card{overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010;padding:16px}.reportbar{display:none}.muted{color:var(--muted);line-height:1.42}.help{display:flex;gap:8px;align-items:center;color:#b9b9b9}.help span{background:#242424;border:1px solid #343434;border-radius:4px;padding:4px 8px}.actions{display:flex;gap:8px;justify-content:flex-end}.action{display:inline-block;min-width:128px;text-align:center;color:#f4f4f4;border:1px solid #3a3a3a;border-radius:4px;background:#262626;font-family:var(--ui-font);font-weight:800;padding:8px 12px}.keep{background:#8d4b17;border-color:#c46b22}.release{background:#1d556c;border-color:#2e7896}.toggle{width:52px;text-align:center}.switch{display:inline-block;width:18px;height:18px;line-height:16px;margin-right:4px;text-align:center;color:#aaa;border:1px solid #343434;border-radius:3px;background:#181818;font-family:'Segoe UI Symbol',var(--ui-font);font-size:11px;font-weight:400;text-decoration:none}.switch:hover{color:#f0f0f0;border-color:#696969;background:#242424}.switch.keep,.switch.release{background:#181818;border-color:#343434}.tablewrap{flex:1;min-height:0;overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010}.modReadme{display:grid!important;grid-template-columns:minmax(0,1.45fr) minmax(260px,.85fr);grid-template-rows:minmax(180px,1fr) minmax(120px,.55fr);gap:12px;height:100%%!important;min-height:0;overflow:hidden}.modReadme .card{height:auto!important;min-height:0;overflow-y:auto;overflow-x:hidden;scrollbar-gutter:auto;padding:14px 14px 16px;background:linear-gradient(135deg,#1d2020 0%%,#181818 58%%,#202020 100%%);border:1px solid rgba(255,255,255,.04);border-radius:5px;box-shadow:inset 0 1px 0 rgba(255,255,255,.03)}.modContrib{grid-template-rows:minmax(150px,.68fr) minmax(220px,1fr)}.settingsGrid{display:grid!important;grid-template-columns:minmax(0,1fr);grid-template-rows:minmax(0,1fr);gap:12px;height:100%%!important;min-height:0;align-content:stretch;overflow:hidden!important}.settingsCard{height:100%%!important;min-height:0;padding:14px 14px 16px;background:#181818;border:1px solid #292929;border-radius:5px;box-shadow:none;overflow-y:auto!important;overflow-x:hidden!important;scrollbar-gutter:stable!important}.flagGroup{margin-top:14px;padding-top:12px;border-top:1px solid #2b2b2b}.flagGroup h3{margin:0;color:#d8d8d8;font-size:13px;font-weight:900;text-transform:uppercase}.flagGroup p{margin:4px 0 8px;color:#9a9a9a;font-size:12px;line-height:1.35}.settingRow{display:grid;grid-template-columns:150px minmax(180px,360px);align-items:center;justify-content:start;gap:12px;margin-top:4px}.settingLabel{color:#9c9c9c;font-size:13px;font-weight:800;white-space:nowrap}.ootpSelect{width:100%%;height:28px;border:1px solid #3c3c3c;border-radius:4px;background:#202020;color:#f0f0f0;font-family:var(--ui-font);font-size:13px;font-weight:700;padding:0 8px}.ootpSelect:focus{outline:1px solid #777;outline-offset:0}.modCard{display:flex;flex-direction:column}.modCardMain{grid-row:1/span 2}.cardTitle{margin:0 0 12px;color:#9c9c9c;font-size:16px;font-weight:900;line-height:1.1;text-transform:uppercase}.modCard p{margin:0 0 10px;color:#eeeeee;line-height:1.48;white-space:normal;font-size:14px;font-weight:400}.modCard p:last-child{margin-bottom:0}.modLead{font-size:15px!important;font-weight:400!important;color:#fff!important}.buildList{display:grid;gap:6px;margin-top:2px;align-content:start}.buildRow{display:grid;grid-template-columns:88px minmax(0,1fr);gap:10px;align-items:baseline;color:#e5e5e5;font-size:13px;line-height:1.35}.buildLabel{color:#8f8f8f;font-weight:900;white-space:nowrap}.buildValue{font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.githubHero{display:flex;align-items:center;gap:13px;margin:2px 0 12px}.githubLogo{width:58px;height:58px;object-fit:contain;filter:invert(54%%) grayscale(1);opacity:.78;padding:4px;flex:none}.githubRepo{min-width:0}.githubRepo strong{display:block;font-size:17px;color:#f4f4f4;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.githubRepo span{display:block;margin-top:4px;color:#9e9e9e;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.githubLink{display:inline-flex;align-items:center;justify-content:center;align-self:flex-start;margin-top:auto;min-width:128px;height:32px;border:1px solid #3a3a3a;border-radius:4px;background:#262626;color:#f4f4f4;font-weight:900}.githubLink:hover{border-color:#686868;background:#303030}@media(max-height:640px){.app{grid-template-rows:78px 34px 34px minmax(0,1fr)!important}.app.noSubTabs{grid-template-rows:78px 34px minmax(0,1fr)!important}.top{padding:0 18px}.logo{width:58px!important;height:58px!important}.mainTabs{height:32px;margin-top:2px}.mainTab{height:24px;min-width:92px;padding:0 10px;font-size:13px}.subTabs{height:32px;padding:0 12px;gap:8px}.subTab{height:22px;line-height:21px;padding:0 12px;font-size:13px}.modReadme{grid-template-rows:minmax(0,1fr)!important}.modReadme .modCard:not(.modCardMain){display:none!important}.modReadme .modCardMain{grid-row:auto!important}}table{width:100%%;table-layout:fixed;border-collapse:separate;border-spacing:0;font-family:var(--ui-font);font-size:13px}th{position:sticky;top:0;background:#222;color:#dedede;text-align:left;padding:8px 10px;border-bottom:1px solid #393939;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}td{padding:7px 10px;border-bottom:1px solid #242424;color:#d8d8d8;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}thead th:first-child{border-top-left-radius:3px}thead th:last-child{border-top-right-radius:3px}tbody tr:last-child td:first-child{border-bottom-left-radius:3px}tbody tr:last-child td:last-child{border-bottom-right-radius:3px}tr:nth-child(even) td{background:#151515}tr:hover td,tr.selected td{background:#243b45;color:#fff}.sel{width:48px;color:var(--orange);font-weight:900;white-space:nowrap}.pname{width:156px;max-width:156px;overflow:hidden;text-overflow:ellipsis;font-weight:800;color:#f3f0e8}.team{width:64px;max-width:64px;text-align:left;font-weight:800;color:#ddd}.flag{width:44px;text-align:center;cursor:help}.flag .roNatFlag{vertical-align:middle}.empty{border:1px dashed rgba(255,255,255,.22);border-radius:4px;padding:22px;color:var(--muted);background:#141414;white-space:normal}"
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

/* ---- native\src\hotkey_window\ui_webview_runtime\webview_types.inc ---- */
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

/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_policy.inc ---- */
static int kbo_webview_team_action_allowed(uint32_t team_id, const char* source)
{
    if (kbo_get_allow_all_ui_team_actions_setting()) {
        return 1;
    }

    if (team_id != 0 && kbo_team_is_human_controlled(team_id, source)) {
        return 1;
    }

    append_logf(
        "webview team action blocked reason=team_not_human_controlled source=%s team=%u",
        source != NULL ? source : "",
        team_id);
    return 0;
}
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_nav_lifetime.inc ---- */
static HRESULT STDMETHODCALLTYPE kbo_webview_nav_qi(ICoreWebView2NavigationStartingEventHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2NavigationStartingEventHandler)) {
        *ppv = This;
        ICoreWebView2NavigationStartingEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_addref(ICoreWebView2NavigationStartingEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewNavHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_release(ICoreWebView2NavigationStartingEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewNavHandler*)This)->ref);
    if (value < 1) { ((KboWebViewNavHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewNavHandler*)This)->ref;
}
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri.inc ---- */
static int kbo_webview_handle_command_uri(const char* uri, HWND hwnd)
{
    if (uri == NULL || strncmp(uri, "kbo://", 6) != 0) {
        return 0;
    }
    const char* cmd = uri + 6;

/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri_parts\external_and_foreign_actions.inc ---- */
    if (ascii_equals_ignore_case(cmd, "github")) {
        ShellExecuteA(
            hwnd,
            "open",
            "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO",
            NULL,
            NULL,
            SW_SHOWNORMAL);
        return 1;
    }
    if (strncmp(cmd, "select/", 7) == 0) {
        uint32_t player_id = (uint32_t)strtoul(cmd + 7, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            append_logf("foreign rights webview: selected player=%u", player_id);
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "retain/", 7) == 0 || strncmp(cmd, "release/", 8) == 0) {
        int retain = strncmp(cmd, "retain/", 7) == 0;
        const char* id_text = retain ? cmd + 7 : cmd + 8;
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            if (kbo_webview_team_action_allowed(g_kbo_hub_selected_team_id, retain ? "hub_foreign_retain" : "hub_foreign_release")) {
                kbo_apply_foreign_rights_button(retain);
            }
        }
        kbo_webview_navigate_current();
        return 1;
    }
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri_parts\view_navigation.inc ---- */
    if (strncmp(cmd, "view/", 5) == 0) {
        int view = atoi(cmd + 5);
        if (view >= 0 && view < KBO_HUB_NAV_COUNT) {
            if (!kbo_hub_view_available_for_selected_league(view)) {
                view = KBO_HUB_VIEW_MOD_INFO;
            }
            if (view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_RIGHTS;
            } else if (view == KBO_HUB_VIEW_UPCOMING_FA) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            } else {
                g_kbo_hub_selected_view = view;
            }
            g_kbo_hub_open_dropdown = 0;
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
                    && (g_kbo_hub_selected_foreign_subview < 0
                        || g_kbo_hub_selected_foreign_subview >= KBO_HUB_FOREIGN_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_ASIAN_GAMES
                    && (g_kbo_hub_selected_agames_subview < 0
                        || g_kbo_hub_selected_agames_subview >= KBO_HUB_AGAMES_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_agames_subview = KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS;
            }
            if (view == KBO_HUB_VIEW_MILITARY
                    && (g_kbo_hub_selected_military_subview < 0
                        || g_kbo_hub_selected_military_subview >= KBO_HUB_MILITARY_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_MOD_INFO
                    && (g_kbo_hub_selected_mod_subview < 0
                        || g_kbo_hub_selected_mod_subview >= KBO_HUB_MOD_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_README;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
                    && (g_kbo_hub_selected_fa_subview < 0
                        || g_kbo_hub_selected_fa_subview >= KBO_HUB_FA_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            }
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/results/year/", 22) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        uint32_t year = (uint32_t)strtoul(cmd + 22, NULL, 10);
        if (year >= 1982u && year <= 2300u) {
            g_kbo_hub_selected_military_results_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
        g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_RESULTS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/", 9) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 9);
        if (subview >= 0 && subview < KBO_HUB_MILITARY_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
            g_kbo_hub_selected_military_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "foreign/", 8) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 8);
        if (subview >= 0 && subview < KBO_HUB_FOREIGN_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
            g_kbo_hub_selected_foreign_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri_parts\mod_settings_commands.inc ---- */
    if (strncmp(cmd, "mod/settings/lang/", 18) == 0) {
        const char* lang = cmd + 18;
        if (ascii_equals_ignore_case(lang, "en") || ascii_equals_ignore_case(lang, "english")) {
            g_kbo_hub_language = KBO_HUB_LANG_EN;
            kbo_hub_save_language_setting();
        } else if (ascii_equals_ignore_case(lang, "ko") || ascii_equals_ignore_case(lang, "korean")) {
            g_kbo_hub_language = KBO_HUB_LANG_KO;
            kbo_hub_save_language_setting();
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/profiler/", 22) == 0) {
        const char* value = cmd + 22;
        int enabled = ascii_equals_ignore_case(value, "on")
            || ascii_equals_ignore_case(value, "1")
            || ascii_equals_ignore_case(value, "true");
        if (kbo_set_profiler_enabled_setting(enabled)) {
            kbo_profiler_reset_enabled_cache();
            append_logf("mod settings webview: profiler enabled=%d", enabled ? 1 : 0);
        } else {
            append_logf("mod settings webview: failed to write profiler enabled=%d", enabled ? 1 : 0);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/ui-team-actions/", 29) == 0) {
        const char* value = cmd + 29;
        int allow_all = ascii_equals_ignore_case(value, "all")
            || ascii_equals_ignore_case(value, "on")
            || ascii_equals_ignore_case(value, "1")
            || ascii_equals_ignore_case(value, "true");
        if (kbo_set_allow_all_ui_team_actions_setting(allow_all)) {
            append_logf("mod settings webview: allow all UI team actions=%d", allow_all ? 1 : 0);
        } else {
            append_logf("mod settings webview: failed to write allow all UI team actions=%d", allow_all ? 1 : 0);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/flag/", 18) == 0) {
        const char* key = cmd + 18;
        const char* slash = strchr(key, '/');
        if (slash != NULL && slash > key) {
            char key_buffer[128] = {0};
            size_t key_len = (size_t)(slash - key);
            if (key_len >= sizeof(key_buffer)) {
                key_len = sizeof(key_buffer) - 1u;
            }
            memcpy(key_buffer, key, key_len);
            key_buffer[key_len] = '\0';

            const char* value = slash + 1;
            int enabled = ascii_equals_ignore_case(value, "on")
                || ascii_equals_ignore_case(value, "1")
                || ascii_equals_ignore_case(value, "true")
                || ascii_equals_ignore_case(value, "enabled");
            const KboModRuntimeFlagSetting* setting = kbo_find_mod_runtime_flag_setting(key_buffer);
            if (setting != NULL && kbo_set_mod_runtime_flag_enabled(setting, enabled)) {
                append_logf("mod settings webview: runtime flag %s enabled=%d", key_buffer, enabled ? 1 : 0);
            } else {
                append_logf("mod settings webview: failed to write runtime flag %s enabled=%d", key_buffer, enabled ? 1 : 0);
            }
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/", 4) == 0) {
        int subview = atoi(cmd + 4);
        if (subview >= 0 && subview < KBO_HUB_MOD_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_selected_mod_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri_parts\event_and_fa_commands.inc ---- */
    if (strncmp(cmd, "agames/", 7) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 7);
        if (subview >= 0 && subview < KBO_HUB_AGAMES_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_GAMES;
            g_kbo_hub_selected_agames_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa/", 3) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 3);
        if (subview >= 0 && subview < KBO_HUB_FA_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
            g_kbo_hub_selected_fa_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/select/", 15) == 0) {
        const char* text = cmd + 15;
        uint32_t fa_player_id = (uint32_t)strtoul(text, NULL, 10);
        const char* slash = strchr(text, '/');
        uint32_t selected_player_id = slash != NULL ? (uint32_t)strtoul(slash + 1, NULL, 10) : 0u;
        if (fa_player_id != 0u && selected_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_original_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_manual_compensation_select")) {
                kbo_manual_select_fa_compensation_player(fa_player_id, selected_player_id, "hub_manual_compensation_select");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/cash-only/", 18) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 18, NULL, 10);
        if (fa_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_original_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_cash_only_select")) {
                kbo_manual_select_fa_compensation_cash_only(fa_player_id, "hub_cash_only_select");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/submit/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_signing_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_protected_list_submit")) {
                kbo_manual_submit_fa_compensation_protected_list(fa_player_id, "hub_protected_list_submit");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/detail/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri_parts\settings_commands.inc ---- */
    if (strncmp(cmd, "settings/intl-fa-multiplier/", 29) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int multiplier = kbo_clamp_intl_established_fa_multiplier(atoi(cmd + 29));
        if (kbo_set_intl_established_fa_multiplier(multiplier)) {
            append_logf("settings webview: international established FA multiplier=%d", multiplier);
        } else {
            append_logf("settings webview: failed to write international established FA multiplier=%d", multiplier);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/foreign-fa-quality-cap/", 32) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* value = cmd + 32;
        int enabled = ascii_equals_ignore_case(value, "on")
            || ascii_equals_ignore_case(value, "1")
            || ascii_equals_ignore_case(value, "true")
            || ascii_equals_ignore_case(value, "enabled");
        if (kbo_set_foreign_fa_quality_cap_enabled_setting(enabled)) {
            append_logf("settings webview: foreign FA quality cap enabled=%d", enabled ? 1 : 0);
        } else {
            append_logf("settings webview: failed to write foreign FA quality cap enabled=%d", enabled ? 1 : 0);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/foreign-fa-baseline/", 29) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* rest = cmd + 29;
        int index = atoi(rest);
        const char* slash = strchr(rest, '/');
        int value = slash != NULL ? atoi(slash + 1) : 0;
        if (kbo_set_foreign_fa_demand_baseline_value(index, value)) {
            append_logf("settings webview: foreign FA demand baseline index=%d value=%d", index, kbo_clamp_foreign_fa_demand_baseline_value(value));
        } else {
            append_logf("settings webview: failed to write foreign FA demand baseline index=%d value=%d", index, value);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/asian-quota-fa-baseline/", 34) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* rest = cmd + 34;
        int index = atoi(rest);
        const char* slash = strchr(rest, '/');
        int value = slash != NULL ? atoi(slash + 1) : 0;
        if (kbo_set_asian_quota_fa_demand_baseline_value(index, value)) {
            append_logf("settings webview: Asian quota FA demand baseline index=%d value=%d", index, kbo_clamp_foreign_fa_demand_baseline_value(value));
        } else {
            append_logf("settings webview: failed to write Asian quota FA demand baseline index=%d value=%d", index, value);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_command_uri_parts\selection_dropdown_commands.inc ---- */
    if (strncmp(cmd, "setleague/", 10) == 0) {
        uint32_t league_id = (uint32_t)strtoul(cmd + 10, NULL, 10);
        if (league_id != 0u) {
            g_kbo_hub_selected_league_id = league_id;
            g_kbo_hub_selected_team_id = 0;
            g_kbo_hub_open_dropdown = 0;
            kbo_hub_ensure_valid_selection();
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "setteam/", 8) == 0) {
        uint32_t team_id = (uint32_t)strtoul(cmd + 8, NULL, 10);
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "league/") == 0 || strcmp(cmd, "league") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 1 ? 0 : 1;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "team/") == 0 || strcmp(cmd, "team") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 2 ? 0 : 2;
        kbo_webview_navigate_current();
        return 1;
    }

    return 1;
}
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_nav_handler.inc ---- */
static HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args)
{
    (void)sender;
    KboWebViewNavHandler* handler = (KboWebViewNavHandler*)This;
    LPWSTR uri_w = NULL;
    if (args == NULL || FAILED(ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &uri_w)) || uri_w == NULL) {
        return S_OK;
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, NULL, 0, NULL, NULL);
    char uri[512] = {0};
    if (needed > 0) {
        WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, uri, sizeof(uri), NULL, NULL);
    }
    CoTaskMemFree(uri_w);
    if (kbo_webview_handle_command_uri(uri, handler->hwnd)) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    }
    return S_OK;
}

static const ICoreWebView2NavigationStartingEventHandlerVtbl g_kbo_webview_nav_vtbl = {
    kbo_webview_nav_qi,
    kbo_webview_nav_addref,
    kbo_webview_nav_release,
    kbo_webview_nav_invoke
};

static KboWebViewNavHandler g_kbo_webview_nav_handler = {
    { &g_kbo_webview_nav_vtbl },
    1,
    NULL
};

static EventRegistrationToken g_kbo_webview_nav_token = {0};

/* ---- native\src\hotkey_window\ui_webview_runtime\webview_com_lifetime.inc ---- */
static HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewEnvHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewEnvHandler*)This)->ref);
    if (value < 1) { ((KboWebViewEnvHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewEnvHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewControllerHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewControllerHandler*)This)->ref);
    if (value < 1) { ((KboWebViewControllerHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewControllerHandler*)This)->ref;
}
/* ---- native\src\hotkey_window\ui_webview_runtime\webview_controller.inc ---- */
static void kbo_webview_set_bounds(HWND hwnd)
{
    if (hwnd == NULL || g_kbo_webview_controller == NULL) {
        return;
    }
    RECT client;
    GetClientRect(hwnd, &client);
    RECT bounds = {0, 0, client.right, client.bottom};
    ICoreWebView2Controller_put_Bounds(g_kbo_webview_controller, bounds);
    ICoreWebView2Controller_put_IsVisible(g_kbo_webview_controller, TRUE);
}

static void kbo_webview_apply_ootp_like_settings(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }

    ICoreWebView2Settings* settings = NULL;
    if (FAILED(ICoreWebView2_get_Settings(g_kbo_webview, &settings)) || settings == NULL) {
        return;
    }

    ICoreWebView2Settings_put_IsStatusBarEnabled(settings, FALSE);
    ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, FALSE);
    ICoreWebView2Settings_put_AreDevToolsEnabled(settings, FALSE);
    ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
    ICoreWebView2Settings_Release(settings);
}

static HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result)
{
    KboWebViewControllerHandler* handler = (KboWebViewControllerHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        append_logf("WebView2 controller create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller = result;
    ICoreWebView2Controller_AddRef(g_kbo_webview_controller);
    ICoreWebView2Controller_get_CoreWebView2(g_kbo_webview_controller, &g_kbo_webview);
    if (g_kbo_webview != NULL) {
        kbo_webview_apply_ootp_like_settings();
        g_kbo_webview_nav_handler.hwnd = handler->hwnd;
        ICoreWebView2_add_NavigationStarting(g_kbo_webview, &g_kbo_webview_nav_handler.iface, &g_kbo_webview_nav_token);
        kbo_webview_navigate_current();
    }
    InterlockedExchange(&g_kbo_webview_ready, 1);
    kbo_webview_set_bounds(handler->hwnd);
    append_log_line("WebView2 F2 rights UI ready");
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_kbo_webview_controller_vtbl = {
    kbo_webview_controller_qi,
    kbo_webview_controller_addref,
    kbo_webview_controller_release,
    kbo_webview_controller_invoke
};

static KboWebViewControllerHandler g_kbo_webview_controller_handler = {
    { &g_kbo_webview_controller_vtbl },
    1,
    NULL
};

/* ---- native\src\hotkey_window\ui_webview_runtime\webview_environment.inc ---- */
static HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Environment* result)
{
    KboWebViewEnvHandler* handler = (KboWebViewEnvHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        append_logf("WebView2 environment create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller_handler.hwnd = handler->hwnd;
    HRESULT hr = ICoreWebView2Environment_CreateCoreWebView2Controller(
        result,
        handler->hwnd,
        &g_kbo_webview_controller_handler.iface);
    if (FAILED(hr)) {
        append_logf("WebView2 CreateCoreWebView2Controller failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    }
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_kbo_webview_env_vtbl = {
    kbo_webview_env_qi,
    kbo_webview_env_addref,
    kbo_webview_env_release,
    kbo_webview_env_invoke
};

static KboWebViewEnvHandler g_kbo_webview_env_handler = {
    { &g_kbo_webview_env_vtbl },
    1,
    NULL
};

/* ---- native\src\hotkey_window\ui_webview_runtime\webview_startup.inc ---- */
static void kbo_start_webview_rights_ui(HWND hwnd)
{
    if (hwnd == NULL || InterlockedCompareExchange(&g_kbo_webview_starting, 1, 0) != 0) {
        kbo_webview_set_bounds(hwnd);
        return;
    }

    HMODULE self_module = NULL;
    char loader_path[MAX_PATH] = {0};
    HMODULE loader = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_start_webview_rights_ui, &self_module)
            && GetModuleFileNameA(self_module, loader_path, sizeof(loader_path)) > 0) {
        char* slash = strrchr(loader_path, '\\');
        if (slash != NULL) {
            slash[1] = '\0';
            strncat(loader_path, "WebView2Loader.dll", sizeof(loader_path) - strlen(loader_path) - 1);
            loader = LoadLibraryA(loader_path);
        }
    }
    if (loader == NULL) {
        loader = LoadLibraryA("WebView2Loader.dll");
    }
    if (loader == NULL) {
        append_logf("WebView2Loader.dll load failed error=%lu", GetLastError());
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    KboCreateCoreWebView2EnvironmentWithOptionsFn create_env =
        (KboCreateCoreWebView2EnvironmentWithOptionsFn)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (create_env == NULL) {
        append_log_line("WebView2 CreateCoreWebView2EnvironmentWithOptions missing");
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    WCHAR user_data[MAX_PATH] = {0};
    WCHAR local[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local, (DWORD)(sizeof(local) / sizeof(local[0])));
    if (got > 0 && got < (DWORD)(sizeof(local) / sizeof(local[0]))) {
        swprintf(user_data, sizeof(user_data) / sizeof(user_data[0]), L"%ls\\OOTP-KBO\\WebView2", local);
        CreateDirectoryW(user_data, NULL);
    }

    g_kbo_webview_env_handler.hwnd = hwnd;
    HRESULT hr = create_env(NULL, user_data[0] != L'\0' ? user_data : NULL, NULL, &g_kbo_webview_env_handler.iface);
    if (FAILED(hr)) {
        append_logf("WebView2 environment start failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    } else {
        append_log_line("WebView2 F2 rights UI starting");
    }
}
/* ---- native\src\hotkey_window\ui_window.inc ---- */

static void kbo_layout_hotkey_window(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_start_webview_rights_ui(hwnd);
    kbo_webview_set_bounds(hwnd);
}

static void kbo_refresh_hotkey_window_layout(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_layout_hotkey_window(hwnd);
    kbo_refresh_hotkey_window();
    if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) != 0) {
        kbo_webview_navigate_current();
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void kbo_hub_get_work_area(HWND hwnd, RECT* out)
{
    if (out == NULL) {
        return;
    }
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (monitor != NULL && GetMonitorInfoA(monitor, &info)) {
        *out = info.rcWork;
        return;
    }
    SystemParametersInfoA(SPI_GETWORKAREA, 0, out, 0);
}

static RECT kbo_hub_fixed_window_rect(HWND hwnd)
{
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE;
    RECT work = {0, 0, 0, 0};
    kbo_hub_get_work_area(hwnd, &work);
    int max_client_w = (work.right > work.left) ? (work.right - work.left - 48) : KBO_HUB_FIXED_CLIENT_WIDTH;
    int max_client_h = (work.bottom > work.top) ? (work.bottom - work.top - 72) : KBO_HUB_FIXED_CLIENT_HEIGHT;
    int client_w = KBO_HUB_FIXED_CLIENT_WIDTH;
    int client_h = KBO_HUB_FIXED_CLIENT_HEIGHT;
    if (max_client_w > 0 && client_w > max_client_w) { client_w = max_client_w; }
    if (max_client_h > 0 && client_h > max_client_h) { client_h = max_client_h; }
    if (client_w < KBO_HUB_MIN_CLIENT_WIDTH) { client_w = KBO_HUB_MIN_CLIENT_WIDTH; }
    if (client_h < KBO_HUB_MIN_CLIENT_HEIGHT) { client_h = KBO_HUB_MIN_CLIENT_HEIGHT; }
    RECT rect = {0, 0, client_w, client_h};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    return rect;
}

static void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position)
{
    if (hwnd == NULL) {
        return;
    }
    RECT rect = kbo_hub_fixed_window_rect(hwnd);
    RECT work = {0, 0, 0, 0};
    kbo_hub_get_work_area(hwnd, &work);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (preserve_position) {
        RECT current = {0, 0, 0, 0};
        if (GetWindowRect(hwnd, &current)) {
            x = current.left;
            y = current.top;
        }
    } else if (work.right > work.left && work.bottom > work.top) {
        x = work.left + ((work.right - work.left) - width) / 2;
        y = work.top + ((work.bottom - work.top) - height) / 2;
    }
    if (work.right > work.left && work.bottom > work.top) {
        if (x + width > work.right) { x = work.right - width; }
        if (y + height > work.bottom) { y = work.bottom - height; }
        if (x < work.left) { x = work.left; }
        if (y < work.top) { y = work.top; }
    }
    SetWindowPos(
        hwnd,
        NULL,
        x,
        y,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        kbo_hub_load_language_setting();
        kbo_hub_init_gdi_objects();
        kbo_hub_ensure_valid_selection();
        SetTimer(hwnd, 1u, 80u, NULL);
        kbo_hub_apply_fixed_window_placement(hwnd, 1);
        kbo_layout_hotkey_window(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        kbo_layout_hotkey_window(hwnd);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc != NULL) {
            RECT client;
            GetClientRect(hwnd, &client);
            FillRect(hdc, &client, g_kbo_hub_brush_bg);
            if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) == 0) {
                RECT text_rect = {22, 22, client.right - 22, 70};
                kbo_hub_draw_text(hdc, "KBO FRONT OFFICE HTML UI LOADING...", text_rect,
                    RGB(245, 241, 231), g_kbo_hub_font_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            kbo_hub_apply_fixed_window_placement(hwnd, 1);
        }
        kbo_refresh_hotkey_window_layout(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = (MINMAXINFO*)lparam;
        if (info != NULL) {
            RECT rect = kbo_hub_fixed_window_rect(hwnd);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            info->ptMinTrackSize.x = width;
            info->ptMinTrackSize.y = height;
            info->ptMaxTrackSize.x = width;
            info->ptMaxTrackSize.y = height;
            info->ptMaxSize.x = width;
            info->ptMaxSize.y = height;
        }
        return 0;
    }

    case WM_COMMAND:
        break;

    case WM_LBUTTONDOWN: {
        POINT point;
        point.x = (int)(short)LOWORD(lparam);
        point.y = (int)(short)HIWORD(lparam);
        if (PtInRect(&g_kbo_hub_league_dropdown_rect, point)) {
            kbo_hub_show_league_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_team_dropdown_rect, point)) {
            kbo_hub_show_team_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_ko_rect, point)) {
            g_kbo_hub_language = KBO_HUB_LANG_KO;
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_en_rect, point)) {
            g_kbo_hub_language = KBO_HUB_LANG_EN;
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_less_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, -1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_more_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, 1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                int direction = point.y < g_kbo_hub_scrollbar_thumb_rect.top
                    ? -kbo_hub_estimate_visible_edit_lines()
                    :  kbo_hub_estimate_visible_edit_lines();
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, direction);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        RECT wnd_rect;
        GetClientRect(hwnd, &wnd_rect);
        int wnd_w = wnd_rect.right - wnd_rect.left;
        int wnd_h = wnd_rect.bottom - wnd_rect.top;
        for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
            RECT item = kbo_hub_nav_item_rect(i, wnd_w, wnd_h);
            if (PtInRect(&item, point)) {
                g_kbo_hub_selected_view = i;
                kbo_refresh_hotkey_window_layout(hwnd);
                return 0;
            }
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wparam;
        SetTextColor(hdc, KBO_HUB_COLOR_TEXT);
        SetBkColor(hdc, KBO_HUB_COLOR_PANEL_ALT);
        return (LRESULT)g_kbo_hub_brush_panel_alt;
    }

    case WM_TIMER:
        if (wparam == 1u
                && ((GetAsyncKeyState(VK_F2) & 1) != 0 || (GetAsyncKeyState(VK_F9) & 1) != 0)
                && kbo_foreground_is_this_process()) {
            kbo_queue_hotkey_window_toggle();
            return 0;
        }
        if (wparam == 1u
                && (GetAsyncKeyState(VK_F5) & 1) != 0
                && g_kbo_hotkey_window != NULL
                && IsWindowVisible(g_kbo_hotkey_window)
                && kbo_foreground_is_this_process()) {
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wparam == 1u && g_kbo_hotkey_window != NULL && IsWindowVisible(g_kbo_hotkey_window)) {
            static uintptr_t s_last_db_ptr = 0;
            uintptr_t cur_db = get_ootp_cached_global_database();
            if (cur_db != s_last_db_ptr) {
                s_last_db_ptr = cur_db;
                kbo_refresh_hotkey_window();
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
        }
        break;

    case KBO_WM_TOGGLE_SERVICE_MONITOR:
        kbo_show_or_hide_hotkey_window();
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1u);
        if (g_kbo_hotkey_keyboard_hook != NULL) {
            UnhookWindowsHookEx(g_kbo_hotkey_keyboard_hook);
            g_kbo_hotkey_keyboard_hook = NULL;
        }
        g_kbo_hotkey_window         = NULL;
        g_kbo_hotkey_edit           = NULL;
        g_kbo_foreign_list          = NULL;
        g_kbo_foreign_keep_button   = NULL;
        g_kbo_foreign_release_button = NULL;
        g_kbo_hotkey_edit_original_proc = NULL;
        kbo_hub_delete_gdi_objects();
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter)
{
    g_kbo_hotkey_instance = (HINSTANCE)parameter;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    kbo_hub_init_gdi_objects();

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = kbo_hotkey_window_proc;
    wc.hInstance     = g_kbo_hotkey_instance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = g_kbo_hub_brush_bg;
    wc.lpszClassName = "OOTPKBOHubWindow";

    ATOM klass = RegisterClassExA(&wc);
    if (klass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        append_logf("KBO F2 hub class registration failed error=%lu", GetLastError());
        return 0;
    }

    HWND owner = kbo_find_ootp_main_window();
    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD window_ex_style = WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE;
    RECT fixed_rect = kbo_hub_fixed_window_rect(owner);
    HWND hwnd = CreateWindowExA(
        window_ex_style,
        wc.lpszClassName,
        "Ultimate KBO",
        window_style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        fixed_rect.right - fixed_rect.left,
        fixed_rect.bottom - fixed_rect.top,
        owner, NULL, g_kbo_hotkey_instance, NULL);

    if (hwnd == NULL) {
        append_logf("KBO F2 hub window creation failed error=%lu", GetLastError());
        return 0;
    }

    g_kbo_hotkey_window = hwnd;
    g_kbo_hotkey_keyboard_hook = SetWindowsHookExA(
        WH_KEYBOARD_LL, kbo_hotkey_keyboard_proc, g_kbo_hotkey_instance, 0);
    if (g_kbo_hotkey_keyboard_hook == NULL) {
        append_logf("KBO F2 hub keyboard hook failed error=%lu; falling back to polling", GetLastError());
    } else {
        append_logf("KBO F2 hub keyboard hook installed hook=%p", (void*)g_kbo_hotkey_keyboard_hook);
    }
    append_logf("KBO F2 hub ready hwnd=%p owner=%p", (void*)hwnd, (void*)owner);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    CoUninitialize();
    return 0;
}

void start_kbo_hotkey_window_thread(HINSTANCE instance)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_hotkey_window_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_hotkey_window_thread, instance, 0, &g_kbo_hotkey_thread_id);
    if (thread == NULL) {
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        append_logf("KBO F2 hub thread failed error=%lu", GetLastError());
        return;
    }

    CloseHandle(thread);
}