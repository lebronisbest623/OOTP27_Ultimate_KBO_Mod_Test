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

static void kbo_webview_append_player_name_cell(KboWindowTextBuffer* buffer, const char* player_name, uint32_t player_id)
{
    (void)player_id;
    kbo_window_text_appendf(buffer, "<td class='pname'>");
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(buffer, "</td>");
}

static void kbo_webview_append_team_cell(KboWindowTextBuffer* buffer, const char* team_text)
{
    kbo_window_text_appendf(buffer, "<td class='team'>");
    kbo_html_append_escaped(buffer, team_text != NULL && team_text[0] != '\0' ? team_text : "-");
    kbo_window_text_appendf(buffer, "</td>");
}

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

static const char* kbo_hub_flag_entity_for_nation_id(uint32_t nation_id, const char** out_label)
{
    const char* label = "Unknown nation";
    const char* flag = "&#x1F3F3;";
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: label = "Korea";             flag = "&#x1F1F0;&#x1F1F7;"; break;
    case 12u:                        label = "Australia";         flag = "&#x1F1E6;&#x1F1FA;"; break;
    case 36u:                        label = "Canada";            flag = "&#x1F1E8;&#x1F1E6;"; break;
    case 43u:                        label = "Taiwan";            flag = "&#x1F1F9;&#x1F1FC;"; break;
    case 49u:                        label = "Cuba";              flag = "&#x1F1E8;&#x1F1FA;"; break;
    case 56u:                        label = "Dominican Republic"; flag = "&#x1F1E9;&#x1F1F4;"; break;
    case 98u:                        label = "Japan";             flag = "&#x1F1EF;&#x1F1F5;"; break;
    case 124u:                       label = "Mexico";            flag = "&#x1F1F2;&#x1F1FD;"; break;
    case 206u:                       label = "United States";     flag = "&#x1F1FA;&#x1F1F8;"; break;
    case 210u:                       label = "Venezuela";         flag = "&#x1F1FB;&#x1F1EA;"; break;
    default: break;
    }
    if (out_label != NULL) {
        *out_label = label;
    }
    return flag;
}

static void kbo_webview_append_nation_flag_cell(KboWindowTextBuffer* buffer, uint32_t nation_id)
{
    const char* label = NULL;
    const char* flag = kbo_hub_flag_entity_for_nation_id(nation_id, &label);
    kbo_window_text_appendf(buffer, "<td class='flag' title='%s nation#%u'>%s</td>",
        label != NULL ? label : "Unknown nation",
        nation_id,
        flag);
}

static void kbo_webview_append_candidate_card(
    KboWindowTextBuffer* buffer,
    uint8_t* player,
    uint32_t player_id,
    uint32_t current_team_id,
    uint32_t nation_id,
    const char* flags)
{
    char player_name[96] = {0};
    char team_abbrev[16] = {0};
    kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
    kbo_hub_copy_team_abbrev_by_id(current_team_id, team_abbrev, sizeof(team_abbrev), NULL);
    kbo_window_text_appendf(buffer,
        "<tr><td class='toggle'><a class='switch keep' title='Retain rights' href='kbo://retain/%u'>&#9679;</a><a class='switch release' title='Do not retain rights' href='kbo://release/%u'>&#9675;</a></td>",
        player_id,
        player_id);
    kbo_webview_append_player_name_cell(buffer, player_name, player_id);
    kbo_webview_append_team_cell(buffer, team_abbrev);
    kbo_webview_append_nation_flag_cell(buffer, nation_id);
    kbo_window_text_appendf(buffer, "<td>");
    kbo_html_append_escaped(buffer, flags != NULL && flags[0] != '\0' ? flags : "Active");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

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
        kbo_hub_copy_league_display_name(leagues[i], label, sizeof(label));
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

