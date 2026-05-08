static void kbo_html_append_escaped(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL || text == NULL) {
        return;
    }
    for (const char* p = text; *p != '\0'; p++) {
        switch (*p) {
        case '&': kbo_window_text_appendf(buffer, "&amp;"); break;
        case '<': kbo_window_text_appendf(buffer, "&lt;"); break;
        case '>': kbo_window_text_appendf(buffer, "&gt;"); break;
        case '"': kbo_window_text_appendf(buffer, "&quot;"); break;
        case '\'': kbo_window_text_appendf(buffer, "&#39;"); break;
        default: kbo_window_text_appendf(buffer, "%c", *p); break;
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

