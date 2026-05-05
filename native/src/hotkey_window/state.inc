typedef struct KboWindowTextBuffer {
    char* data;
    size_t capacity;
    size_t length;
} KboWindowTextBuffer;

typedef struct KboMainWindowSearch {
    DWORD pid;
    HWND hwnd;
} KboMainWindowSearch;

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
static int g_kbo_hub_selected_view = 0;
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

#define KBO_WM_TOGGLE_SERVICE_MONITOR (WM_APP + 0x4b0u)
#define KBO_HUB_CONTROL_EDIT            3
#define KBO_HUB_CONTROL_FOREIGN_LIST    41
#define KBO_HUB_CONTROL_FOREIGN_KEEP    42
#define KBO_HUB_CONTROL_FOREIGN_RELEASE 43

static HHOOK     g_kbo_game_getmsg_hook      = NULL;
#define KBO_HUB_VIEW_OVERVIEW  0
#define KBO_HUB_VIEW_MILITARY  1
#define KBO_HUB_VIEW_FOREIGN_RIGHTS 2
#define KBO_HUB_VIEW_ASIAN_QUOTA 3
#define KBO_HUB_VIEW_ASIAN_GAMES 4
#define KBO_HUB_VIEW_MOD_INFO    5
#define KBO_HUB_VIEW_SETTINGS    6
#define KBO_HUB_NAV_COUNT        7
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

static int kbo_hub_get_team_vector(uintptr_t* out_vector, int32_t* out_count)
{
    if (out_vector == NULL || out_count == NULL) {
        return 0;
    }
    *out_vector = 0;
    *out_count = 0;

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    *out_vector = team_vector;
    *out_count = team_count;
    return 1;
}

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

static const char* kbo_hub_nav_label(int index)
{
    switch (index) {
    case KBO_HUB_VIEW_OVERVIEW:       return kbo_hub_text("\xed\x99\x88",      "HOME");
    case KBO_HUB_VIEW_MILITARY:       return kbo_hub_text("\xea\xb5\xb0\xeb\xb3\xb5\xeb\xac\xb4",  "SERVICE");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c",  "RIGHTS");
    case KBO_HUB_VIEW_ASIAN_QUOTA:    return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x84\xec\xbf\xbc\xed\x84\xb0", "ASIAN");
    case KBO_HUB_VIEW_ASIAN_GAMES:    return kbo_hub_text("AGAMES", "AGAMES");
    case KBO_HUB_VIEW_MOD_INFO:       return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_SETTINGS:       return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",    "SETTINGS");
    default:                          return "";
    }
}

static int kbo_utf8_to_wide(const char* text, WCHAR* out, int out_count)
{
    if (text == NULL || out == NULL || out_count <= 0) {
        return 0;
    }

    int wrote = MultiByteToWideChar(CP_UTF8, 0, text, -1, out, out_count);
    if (wrote <= 0) {
        wrote = MultiByteToWideChar(CP_ACP, 0, text, -1, out, out_count);
    }
    if (wrote <= 0) {
        out[0] = L'\0';
        return 0;
    }
    return 1;
}

static void kbo_hub_trim_ascii(char* text)
{
    if (text == NULL) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\r' || text[len - 1] == '\n'
                       || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

static uint32_t  g_kbo_league_ptr_cache_id  = 0;
static uintptr_t g_kbo_league_ptr_cache_ptr = 0;

static uintptr_t kbo_find_league_ptr(uint32_t league_id)
{
    if (league_id == 0) {
        return 0;
    }

    /* Fast path: cached value still valid */
    if (g_kbo_league_ptr_cache_id == league_id && g_kbo_league_ptr_cache_ptr != 0) {
        uintptr_t cached = g_kbo_league_ptr_cache_ptr;
        if (memory_range_readable((void*)cached, OOTP27_KBO_LEAGUE_ID_OFFSET + 4u)
                && *(uint32_t*)((uint8_t*)cached + OOTP27_KBO_LEAGUE_ID_OFFSET) == league_id) {
            return cached;
        }
        g_kbo_league_ptr_cache_id  = 0;
        g_kbo_league_ptr_cache_ptr = 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        append_logf("KBO: league search id=%u - failed to get global database", league_id);
        return 0;
    }

    append_logf("KBO: league search id=%u - scanning from global=%p", league_id, (void*)global);

    /* Try multiple possible offsets for league vector in global DB. */
    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u, 0xe0u, 0xe8u, 0xf0u, 0xf8u,
        0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    uintptr_t team_vector = 0;
    int32_t   team_count  = 0;
    /* Get team vector to use for filtering (we'll skip this in our search) */
    if (kbo_hub_get_team_vector(&team_vector, &team_count)) {
        append_logf("KBO: league search id=%u - team_vector=%p for reference", league_id, (void*)team_vector);
    }

    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;  /* Count is usually 8 bytes after vector pointer */
        
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }
        
        uintptr_t candidate_vec = *(uintptr_t*)((uint8_t*)global + vec_off);
        int32_t candidate_count = *(int32_t*)((uint8_t*)global + cnt_off);
        
        if (candidate_vec == 0 || candidate_vec == team_vector) {
            continue; /* skip empty or the team vector */
        }
        if (candidate_count < 1 || candidate_count > 500) {
            continue;
        }
        if (!memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }
        
        append_logf("KBO: league search id=%u - trying offset 0x%x: vec=%p count=%d", 
            league_id, vec_off, (void*)candidate_vec, candidate_count);
        
        /* Scan all entries in this vector for matching league ID */
        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t league_candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 4u)) {
                continue;
            }
            
            uint32_t cand_id = *(uint32_t*)((uint8_t*)league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
            if (cand_id == league_id) {
                uint32_t cand_year = 0;
                if (memory_range_readable((void*)(league_candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), 4u)) {
                    cand_year = *(uint32_t*)((uint8_t*)league_candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
                }
                append_logf("KBO: league ptr FOUND id=%u ptr=%p year=%u (offset 0x%x index %d)", 
                    league_id, (void*)league_candidate, cand_year, vec_off, j);
                g_kbo_league_ptr_cache_id  = league_id;
                g_kbo_league_ptr_cache_ptr = league_candidate;
                return league_candidate;
            }
        }
    }
    
    append_logf("KBO: league search id=%u - FAILED to find in any vector", league_id);
    return 0;
}

static int kbo_league_name_char_ok(unsigned char c)
{
    /* Allow printable ASCII (0x20-0x7E) and all UTF-8 continuation bytes/multi-byte chars (0x80+)
     * This more permissive check allows Korean and other multi-byte UTF-8 sequences. */
    return (c >= 0x20u && c <= 0x7Eu) || c >= 0x80u;
}

/* Scans common string-object offsets within a league object and copies the
 * first string that looks like a league name into out. */
static void kbo_hub_read_league_name(uintptr_t league_ptr, char* out, size_t out_size)
{
    if (league_ptr == 0 || out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    static const uint32_t try_offsets[] = {
        0x00u, 0x08u, 0x10u, 0x18u, 0x20u, 0x28u, 0x30u, 0x38u,
        0x40u, 0x48u, 0x50u, 0x58u, 0x60u, 0x68u, 0x70u, 0x78u,
        0x80u, 0x88u, 0x90u, 0x98u, 0xa0u, 0xa8u, 0xb0u, 0xb8u,
        0xc0u, 0xc8u, 0xd0u, 0xd8u, 0xe0u, 0xe8u, 0xf0u, 0xf8u,
    };

    uint8_t* league = (uint8_t*)league_ptr;
    char text[128]  = {0};
    append_logf("KBO: scanning league name at %p", league_ptr);

    for (size_t i = 0; i < sizeof(try_offsets) / sizeof(try_offsets[0]); i++) {
        text[0] = '\0';
        if (!copy_ootp_string_object_text(league, try_offsets[i], text, sizeof(text))) {
            append_logf("KBO:   offset 0x%02x - copy_ootp_string_object_text failed", try_offsets[i]);
            continue;
        }
        size_t len = strlen(text);
        append_logf("KBO:   offset 0x%02x - got text len=%zu: %s", try_offsets[i], len, text);
        
        if (len < 2 || len > 60) {
            append_logf("KBO:   offset 0x%02x - REJECT: length check failed (len=%zu)", try_offsets[i], len);
            continue;
        }
        /* Must start with a letter (ASCII or UTF-8 multi-byte). */
        unsigned char first = (unsigned char)text[0];
        if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first >= 0x80u)) {
            append_logf("KBO:   offset 0x%02x - REJECT: first char not a letter (0x%02x)", try_offsets[i], first);
            continue;
        }
        /* All bytes must be printable ASCII or UTF-8 continuation. */
        int clean = 1;
        for (size_t j = 0; j < len; j++) {
            if (!kbo_league_name_char_ok((unsigned char)text[j])) {
                append_logf("KBO:   offset 0x%02x - REJECT: invalid char at pos %zu (0x%02x)", 
                    try_offsets[i], j, (unsigned char)text[j]);
                clean = 0; break;
            }
        }
        if (!clean) {
            continue;
        }
        append_logf("KBO:   offset 0x%02x - ACCEPT: %s", try_offsets[i], text);
        snprintf(out, out_size, "%s", text);
        return;
    }
    append_logf("KBO: league name scan FAILED - no valid string found at %p", league_ptr);
}

static void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("리그 없음", "No league"));
        return;
    }

    uintptr_t league_ptr = kbo_find_league_ptr(league_id);
    if (league_ptr == 0) {
        snprintf(out, out_size, "%s %u", kbo_hub_text("리그", "League"), league_id);
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
        snprintf(out, out_size, "%s %u / %u", kbo_hub_text("리그", "League"), league_id, year);
    } else {
        snprintf(out, out_size, "%s %u", kbo_hub_text("리그", "League"), league_id);
    }
}

static void kbo_hub_ensure_valid_selection(void);
static void kbo_refresh_hotkey_window(void);
static int kbo_resolve_foreign_waiver_top_candidate_for_team(uint32_t team_id, uint32_t* out_player_id, uint32_t* out_current_team_id);
static int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain);
static int kbo_is_foreign_waiver_negotiation_window_open(void);
