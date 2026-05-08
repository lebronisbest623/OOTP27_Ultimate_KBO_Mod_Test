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
