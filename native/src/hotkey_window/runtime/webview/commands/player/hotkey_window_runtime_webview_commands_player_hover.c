#include "../../hotkey_window_webview_internal.h"
#include "../../../player_hover/player_hover_manager_probe.h"
#include "../../../../support/assets/paths/ui_image_sources.h"
#include "../../../../support/text/buffer/ui_text_buffer.h"
#include "../../../../support/text/js/ui_js_string.h"
#include "../../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"

static void kbo_webview_execute_utf8_script(const char* script)
{
    if (script == NULL || script[0] == '\0' || g_kbo_webview == NULL) {
        return;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, script, -1, NULL, 0);
    if (wide_len <= 0) {
        wide_len = MultiByteToWideChar(CP_ACP, 0, script, -1, NULL, 0);
        if (wide_len <= 0) {
            return;
        }
    }

    WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
    if (wide == NULL) {
        return;
    }

    int wrote = MultiByteToWideChar(CP_UTF8, 0, script, -1, wide, wide_len);
    if (wrote <= 0) {
        wrote = MultiByteToWideChar(CP_ACP, 0, script, -1, wide, wide_len);
    }
    if (wrote > 0) {
        ICoreWebView2_ExecuteScript(g_kbo_webview, wide, NULL);
    }
    HeapFree(GetProcessHeap(), 0, wide);
}

static void kbo_show_webview_player_tooltip(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    const char* player_name,
    int client_x,
    int client_y,
    uint32_t hover_seq)
{
    (void)player;
    char payload[12000] = {0};
    if (!kbo_capture_ootp_player_tooltip_payload(player_id, payload, sizeof(payload))) {
        snprintf(
            payload,
            sizeof(payload),
            "%s\nplayer_id: %u\nteam_id: %u\nleague_id: %u",
            player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player",
            player_id,
            team_id,
            league_id);
    }

    char script_data[20000] = {0};
    KboWindowTextBuffer script = { script_data, sizeof(script_data), 0u };
    kbo_window_text_appendf(
        &script,
        "(function(){"
        "var tip=document.getElementById('kboPlayerTooltip');"
        "if(!tip){tip=document.createElement('pre');tip.id='kboPlayerTooltip';document.body.appendChild(tip);}"
        "tip.textContent=");
    kbo_webview_append_js_string(&script, payload);
    kbo_window_text_appendf(
        &script,
        ";tip.dataset.hoverSeq='%u';"
        "tip.style.cssText='position:fixed;display:block;z-index:2147483647;max-width:520px;max-height:520px;overflow:auto;"
        "white-space:pre-wrap;background:#16191d;color:#f5f1e8;border:1px solid rgba(255,255,255,.18);"
        "box-shadow:0 12px 32px rgba(0,0,0,.45);border-radius:6px;padding:10px 12px;font:12px/1.45 Consolas,monospace;pointer-events:none;';"
        "tip.style.left=Math.max(8,Math.min(window.innerWidth-80,%d+14))+'px';"
        "tip.style.top=Math.max(8,Math.min(window.innerHeight-80,%d+14))+'px';"
        "})();",
        hover_seq,
        client_x,
        client_y);

    kbo_webview_execute_utf8_script(script.data);
}

static void kbo_hide_webview_player_tooltip(void)
{
    kbo_webview_execute_utf8_script(
        "(function(){var tip=document.getElementById('kboPlayerTooltip');if(tip){tip.style.display='none';}})();");
}

static int kbo_parse_u32_segment(const char** cursor, uint32_t* out)
{
    if (cursor == NULL || *cursor == NULL || out == NULL) {
        return 0;
    }

    char* end = NULL;
    unsigned long value = strtoul(*cursor, &end, 10);
    if (end == *cursor || value > 0xfffffffful) {
        return 0;
    }

    *out = (uint32_t)value;
    *cursor = end;
    if (**cursor == '/') {
        *cursor += 1;
    }
    return 1;
}

static void kbo_request_player_hover_payload(HWND hwnd, uint32_t player_id, int client_x, int client_y, uint32_t hover_seq)
{
    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(player_id, &team_id, &league_id);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_log_runtimef(
            "webview player hover ignored reason=player_not_found player=%u x=%d y=%d",
            player_id,
            client_x,
            client_y);
        return;
    }

    char player_name[128] = {0};
    kbo_copy_player_display_name(player, player_name, sizeof(player_name));

    kbo_log_runtimef(
        "webview player hover payload player=%u seq=%u name=\"%s\" team=%u league=%u hwnd=%p x=%d y=%d manager=%p",
        player_id,
        hover_seq,
        player_name,
        team_id,
        league_id,
        hwnd,
        client_x,
        client_y,
        (void*)kbo_player_hover_manager_ptr());
    kbo_show_webview_player_tooltip(player, player_id, team_id, league_id, player_name, client_x, client_y, hover_seq);
}

static void kbo_hide_player_hover_payload(HWND hwnd, uint32_t player_id)
{
    (void)hwnd;
    kbo_hide_webview_player_tooltip();
    kbo_log_runtimef("webview player hover hide player=%u", player_id);
}

int kbo_webview_handle_player_hover_command(const char* cmd, HWND hwnd)
{
    if (cmd == NULL) {
        return 0;
    }

    if (strncmp(cmd, "player-hover/debug/", 19) == 0) {
        const char* cursor = cmd + 19;
        uint32_t hover_seq = 0u;
        uint32_t player_id = 0u;
        uint32_t ratings = 0u;
        uint32_t bars = 0u;
        uint32_t fills = 0u;
        uint32_t scale = 0u;
        uint32_t first_pct = 0u;
        (void)hwnd;
        if (!kbo_parse_u32_segment(&cursor, &hover_seq)
                || !kbo_parse_u32_segment(&cursor, &player_id)) {
            return 1;
        }
        (void)kbo_parse_u32_segment(&cursor, &ratings);
        (void)kbo_parse_u32_segment(&cursor, &bars);
        (void)kbo_parse_u32_segment(&cursor, &fills);
        (void)kbo_parse_u32_segment(&cursor, &scale);
        (void)kbo_parse_u32_segment(&cursor, &first_pct);
        kbo_log_runtimef(
            "webview player tooltip render debug seq=%u player=%u ratings=%u bars=%u fills=%u scale=%u first_pct=%u",
            hover_seq,
            player_id,
            ratings,
            bars,
            fills,
            scale,
            first_pct);
        return 1;
    }

    if (strncmp(cmd, "player-hover/show/", 18) == 0) {
        const char* cursor = cmd + 18;
        uint32_t player_id = 0u;
        uint32_t client_x = 0u;
        uint32_t client_y = 0u;
        uint32_t hover_seq = 0u;
        if (!kbo_parse_u32_segment(&cursor, &player_id)
                || !kbo_parse_u32_segment(&cursor, &client_x)
                || !kbo_parse_u32_segment(&cursor, &client_y)
                || player_id == 0u) {
            return 1;
        }
        (void)kbo_parse_u32_segment(&cursor, &hover_seq);

        kbo_request_player_hover_payload(hwnd, player_id, (int)client_x, (int)client_y, hover_seq);
        return 1;
    }

    if (strncmp(cmd, "player-hover/hide/", 18) == 0) {
        const char* cursor = cmd + 18;
        uint32_t player_id = 0u;
        if (kbo_parse_u32_segment(&cursor, &player_id) && player_id != 0u) {
            kbo_hide_player_hover_payload(hwnd, player_id);
        }
        return 1;
    }

    return 0;
}
